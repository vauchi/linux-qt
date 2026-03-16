// SPDX-FileCopyrightText: 2026 Mattia Egloff <mattia.egloff@pm.me>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QObject>
#include <QByteArray>
#include <QAudioSink>
#include <QAudioSource>
#include <QBuffer>
#include <QThread>
#include <atomic>
#include <cstdint>
#include <vector>

class HardwareBackend;

/// Ultrasonic audio backend for proximity verification (ADR-031).
///
/// Implements FSK-modulated ultrasonic signals matching the vauchi-core
/// audio protocol:
///   - Carrier: 18.5 kHz, shift: 1 kHz
///   - Preamble: 50ms at 19 kHz
///   - Bit duration: 10ms (100 bps)
///   - Goertzel algorithm for frequency detection
///
/// Uses Qt Multimedia (QAudioSink/QAudioSource) for audio I/O.
/// All operations run on a worker thread to avoid blocking the UI.
class AudioBackend : public QObject {
    Q_OBJECT

public:
    explicit AudioBackend(HardwareBackend *parent);
    ~AudioBackend() override;

    /// Emit an ultrasonic challenge signal encoding `data`.
    void emitChallenge(const QByteArray &data);

    /// Listen for an ultrasonic response within `timeoutMs`.
    void listenForResponse(uint32_t timeoutMs);

    /// Stop any active audio operation.
    void stop();

    /// Returns true if audio output and input devices are available.
    static bool isAvailable();

signals:
    /// Internal: runs emit on worker thread.
    void doEmit(QByteArray data);
    /// Internal: runs listen on worker thread.
    void doListen(uint32_t timeoutMs);

private:
    // FSK protocol constants (must match vauchi-core audio_cpal.rs)
    static constexpr float kCarrierFreq = 18500.0f;
    static constexpr float kFreqShift = 1000.0f;
    static constexpr float kPreambleFreq = 19000.0f;
    static constexpr uint32_t kSampleRate = 44100;
    static constexpr float kAmplitude = 0.8f;

    /// Generate FSK-modulated PCM samples for the given data bytes.
    static std::vector<float> generateFskSamples(const QByteArray &data);

    /// Decode FSK-modulated PCM samples back to data bytes.
    static QByteArray decodeFskSamples(const std::vector<float> &samples);

    /// Find the start of the preamble (19 kHz burst) in samples.
    static int findPreamble(const std::vector<float> &samples);

    /// Goertzel algorithm for single-frequency power detection.
    static float goertzel(const float *samples, int count, float targetFreq);

    HardwareBackend *m_backend;
    QThread m_workerThread;
    std::atomic<bool> m_stopRequested{false};
};
