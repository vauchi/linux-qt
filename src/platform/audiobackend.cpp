// SPDX-FileCopyrightText: 2026 Mattia Egloff <mattia.egloff@pm.me>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "audiobackend.h"
#include "hardwarebackend.h"

#include <QAudioFormat>
#include <QMediaDevices>
#include <QJsonObject>
#include <QJsonArray>
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QtMath>

#include <cstring>

// ── Construction / destruction ──────────────────────────────────────

AudioBackend::AudioBackend(HardwareBackend *parent)
    : QObject(nullptr), m_backend(parent) {
    // Move to worker thread so emit/listen run off the UI thread.
    this->moveToThread(&m_workerThread);

    connect(this, &AudioBackend::doEmit, this, [this](QByteArray data) {
        m_stopRequested.store(false);

        auto samples = generateFskSamples(data);

        // Convert float samples to 16-bit PCM for QAudioSink
        QByteArray pcmBuf(static_cast<int>(samples.size()) * 2, Qt::Uninitialized);
        auto *pcm = reinterpret_cast<int16_t *>(pcmBuf.data());
        for (size_t i = 0; i < samples.size(); ++i) {
            pcm[i] = static_cast<int16_t>(samples[i] * 32767.0f);
        }

        QAudioFormat fmt;
        fmt.setSampleRate(kSampleRate);
        fmt.setChannelCount(1);
        fmt.setSampleFormat(QAudioFormat::Int16);

        auto outputDevice = QMediaDevices::defaultAudioOutput();
        if (outputDevice.isNull()) {
            QJsonObject event, inner;
            inner["transport"] = QStringLiteral("Audio");
            inner["error"] = QStringLiteral("No audio output device");
            event["HardwareError"] = inner;
            QMetaObject::invokeMethod(m_backend, [this, event]() {
                m_backend->sendHardwareEvent(event);
            }, Qt::QueuedConnection);
            return;
        }

        QAudioSink sink(outputDevice, fmt);
        QBuffer buffer(&pcmBuf);
        buffer.open(QIODevice::ReadOnly);
        sink.start(&buffer);

        // Wait for playback to finish
        while (sink.state() != QAudio::IdleState && !m_stopRequested.load()) {
            QThread::msleep(10);
            // Process events so QAudioSink can do its work
            QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
        }

        sink.stop();
    });

    connect(this, &AudioBackend::doListen, this, [this](uint32_t timeoutMs) {
        m_stopRequested.store(false);

        QAudioFormat fmt;
        fmt.setSampleRate(kSampleRate);
        fmt.setChannelCount(1);
        fmt.setSampleFormat(QAudioFormat::Int16);

        auto inputDevice = QMediaDevices::defaultAudioInput();
        if (inputDevice.isNull()) {
            QJsonObject event, inner;
            inner["transport"] = QStringLiteral("Audio");
            inner["error"] = QStringLiteral("No audio input device");
            event["HardwareError"] = inner;
            QMetaObject::invokeMethod(m_backend, [this, event]() {
                m_backend->sendHardwareEvent(event);
            }, Qt::QueuedConnection);
            return;
        }

        QAudioSource source(inputDevice, fmt);
        QByteArray capturedPcm;
        capturedPcm.reserve(static_cast<int>(kSampleRate * timeoutMs / 1000) * 2);

        auto *ioDevice = source.start();
        QElapsedTimer timer;
        timer.start();

        // Capture audio for up to timeoutMs, trying to decode periodically
        while (!m_stopRequested.load()
               && timer.elapsed() < static_cast<qint64>(timeoutMs)) {
            QThread::msleep(50);
            QCoreApplication::processEvents(QEventLoop::AllEvents, 10);

            QByteArray chunk = ioDevice->readAll();
            if (!chunk.isEmpty()) {
                capturedPcm.append(chunk);
            }

            // Try to decode once we have at least 0.5s of audio
            if (capturedPcm.size() >= static_cast<int>(kSampleRate)) {
                // Convert 16-bit PCM to float
                int sampleCount = capturedPcm.size() / 2;
                std::vector<float> floatSamples(sampleCount);
                auto *raw = reinterpret_cast<const int16_t *>(capturedPcm.constData());
                for (int i = 0; i < sampleCount; ++i) {
                    floatSamples[i] = static_cast<float>(raw[i]) / 32768.0f;
                }

                QByteArray decoded = decodeFskSamples(floatSamples);
                if (!decoded.isEmpty()) {
                    source.stop();

                    QJsonObject event, inner;
                    QJsonArray dataArr;
                    for (char byte : decoded) {
                        dataArr.append(static_cast<int>(static_cast<unsigned char>(byte)));
                    }
                    inner["data"] = dataArr;
                    event["AudioResponseReceived"] = inner;

                    QMetaObject::invokeMethod(m_backend, [this, event]() {
                        m_backend->sendHardwareEvent(event);
                    }, Qt::QueuedConnection);
                    return;
                }
            }
        }

        source.stop();

        // Final decode attempt on all captured audio
        if (!capturedPcm.isEmpty()) {
            int sampleCount = capturedPcm.size() / 2;
            std::vector<float> floatSamples(sampleCount);
            auto *raw = reinterpret_cast<const int16_t *>(capturedPcm.constData());
            for (int i = 0; i < sampleCount; ++i) {
                floatSamples[i] = static_cast<float>(raw[i]) / 32768.0f;
            }

            QByteArray decoded = decodeFskSamples(floatSamples);
            if (!decoded.isEmpty()) {
                QJsonObject event, inner;
                QJsonArray dataArr;
                for (char byte : decoded) {
                    dataArr.append(static_cast<int>(static_cast<unsigned char>(byte)));
                }
                inner["data"] = dataArr;
                event["AudioResponseReceived"] = inner;

                QMetaObject::invokeMethod(m_backend, [this, event]() {
                    m_backend->sendHardwareEvent(event);
                }, Qt::QueuedConnection);
                return;
            }
        }

        // Timed out without decoding a signal
        QJsonObject event, inner;
        inner["transport"] = QStringLiteral("Audio");
        inner["error"] = QStringLiteral("No ultrasonic signal detected within timeout");
        event["HardwareError"] = inner;
        QMetaObject::invokeMethod(m_backend, [this, event]() {
            m_backend->sendHardwareEvent(event);
        }, Qt::QueuedConnection);
    });

    m_workerThread.start();
}

AudioBackend::~AudioBackend() {
    m_stopRequested.store(true);
    m_workerThread.quit();
    m_workerThread.wait();
}

// ── Public API ──────────────────────────────────────────────────────

void AudioBackend::emitChallenge(const QByteArray &data) {
    emit doEmit(data);
}

void AudioBackend::listenForResponse(uint32_t timeoutMs) {
    emit doListen(timeoutMs);
}

void AudioBackend::stop() {
    m_stopRequested.store(true);
}

bool AudioBackend::isAvailable() {
    return !QMediaDevices::defaultAudioOutput().isNull()
        && !QMediaDevices::defaultAudioInput().isNull();
}

// ── FSK signal generation ───────────────────────────────────────────

std::vector<float> AudioBackend::generateFskSamples(const QByteArray &data) {
    const float sampleRate = static_cast<float>(kSampleRate);

    // Bit duration: 10ms = 100 bps
    const int samplesPerBit = static_cast<int>(sampleRate * 0.01f);

    // Preamble: 50ms at 19 kHz
    const int preambleSamples = static_cast<int>(sampleRate * 0.05f);

    // Gap after preamble: 5ms
    const int gapSamples = static_cast<int>(sampleRate * 0.005f);

    std::vector<float> samples;
    samples.reserve(preambleSamples + gapSamples
                    + data.size() * 8 * samplesPerBit + gapSamples);

    // Generate preamble
    for (int i = 0; i < preambleSamples; ++i) {
        float t = static_cast<float>(i) / sampleRate;
        samples.push_back(qSin(2.0f * M_PI * kPreambleFreq * t) * kAmplitude);
    }

    // Gap
    samples.insert(samples.end(), gapSamples, 0.0f);

    // Generate FSK data (MSB first, matching Rust implementation)
    for (int byteIdx = 0; byteIdx < data.size(); ++byteIdx) {
        auto byte = static_cast<unsigned char>(data[byteIdx]);
        for (int bitIdx = 0; bitIdx < 8; ++bitIdx) {
            int bit = (byte >> (7 - bitIdx)) & 1;
            float freq = bit ? (kCarrierFreq + kFreqShift) : kCarrierFreq;

            for (int i = 0; i < samplesPerBit; ++i) {
                float t = static_cast<float>(i) / sampleRate;
                samples.push_back(qSin(2.0f * M_PI * freq * t) * kAmplitude);
            }
        }
    }

    // Trailing silence
    samples.insert(samples.end(), gapSamples, 0.0f);

    return samples;
}

// ── FSK signal decoding ─────────────────────────────────────────────

QByteArray AudioBackend::decodeFskSamples(const std::vector<float> &samples) {
    const float sampleRate = static_cast<float>(kSampleRate);
    const int samplesPerBit = static_cast<int>(sampleRate * 0.01f);

    int preambleStart = findPreamble(samples);
    if (preambleStart < 0) return {};

    // Skip preamble (50ms) + gap (5ms)
    int dataStart = preambleStart + static_cast<int>(sampleRate * 0.055f);

    if (dataStart >= static_cast<int>(samples.size())) return {};

    QByteArray result;
    unsigned char currentByte = 0;
    int bitCount = 0;
    int sampleIdx = dataStart;

    while (sampleIdx + samplesPerBit <= static_cast<int>(samples.size())) {
        const float *chunk = &samples[sampleIdx];

        float powerCarrier = goertzel(chunk, samplesPerBit, kCarrierFreq);
        float powerShift = goertzel(chunk, samplesPerBit, kCarrierFreq + kFreqShift);

        // Check if signal is present (above noise floor)
        const float threshold = 0.01f;
        if (powerCarrier < threshold && powerShift < threshold) break;

        int bit = (powerShift > powerCarrier) ? 1 : 0;

        currentByte = static_cast<unsigned char>((currentByte << 1) | bit);
        bitCount++;

        if (bitCount == 8) {
            result.append(static_cast<char>(currentByte));
            currentByte = 0;
            bitCount = 0;
        }

        sampleIdx += samplesPerBit;
    }

    return result;
}

int AudioBackend::findPreamble(const std::vector<float> &samples) {
    const float sampleRate = static_cast<float>(kSampleRate);
    const int windowSize = static_cast<int>(sampleRate * 0.01f); // 10ms windows
    const float threshold = 0.05f;
    const int total = static_cast<int>(samples.size());

    for (int start = 0; start + windowSize <= total; start += windowSize / 2) {
        float power = goertzel(&samples[start], windowSize, kPreambleFreq);

        if (power > threshold) {
            // Found preamble — scan back to find exact start
            int scanStart = qMax(0, start - windowSize);
            int miniWindow = qMax(1, windowSize / 4);
            for (int i = scanStart; i < start; ++i) {
                int len = qMin(miniWindow, total - i);
                if (goertzel(&samples[i], len, kPreambleFreq) > threshold / 2.0f) {
                    return i;
                }
            }
            return start;
        }
    }

    return -1; // Not found
}

float AudioBackend::goertzel(const float *samples, int count, float targetFreq) {
    const float sampleRate = static_cast<float>(kSampleRate);
    float k = qRound(targetFreq * count / sampleRate);
    float w = 2.0f * M_PI * k / static_cast<float>(count);
    float coeff = 2.0f * qCos(w);

    float s1 = 0.0f;
    float s2 = 0.0f;

    for (int i = 0; i < count; ++i) {
        float s0 = samples[i] + coeff * s1 - s2;
        s2 = s1;
        s1 = s0;
    }

    float power = s1 * s1 + s2 * s2 - coeff * s1 * s2;
    return qSqrt(power / static_cast<float>(count * count));
}
