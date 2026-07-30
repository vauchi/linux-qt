// SPDX-FileCopyrightText: 2026 Mattia Egloff <mattia.egloff@pm.me>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "presentationsurface.h"

#include <QJsonArray>
#include <QLabel>
#include <QLineEdit>
#include <QPainter>
#include <QPixmap>
#include <QTimer>
#include <QVBoxLayout>
#include <qrencode.h>

namespace {

constexpr int qrExtent = 200;
constexpr int quietZoneModules = 4;
constexpr int frameIntervalMilliseconds = 250;

QPixmap encodeQr(const QString &payload) {
    const QByteArray bytes = payload.toUtf8();
    QRcode *code =
        QRcode_encodeString(bytes.constData(), 0, QR_ECLEVEL_M, QR_MODE_8, 1);
    if (code == nullptr || code->width <= 0) {
        if (code != nullptr) {
            QRcode_free(code);
        }
        return {};
    }

    const int modules = code->width;
    const int extentWithQuietZone = modules + quietZoneModules * 2;
    const int scale = qMax(1, qrExtent / extentWithQuietZone);
    const int imageExtent = extentWithQuietZone * scale;
    const int offset = quietZoneModules * scale;
    QPixmap image(imageExtent, imageExtent);
    image.fill(Qt::white);

    QPainter painter(&image);
    painter.setPen(Qt::NoPen);
    painter.setBrush(Qt::black);
    for (int y = 0; y < modules; ++y) {
        for (int x = 0; x < modules; ++x) {
            if ((code->data[y * modules + x] & 1U) != 0U) {
                painter.drawRect(offset + x * scale, offset + y * scale, scale,
                                 scale);
            }
        }
    }
    painter.end();
    QRcode_free(code);
    return image;
}

} // namespace

QWidget *PresentationSurface::renderQr(const QJsonObject &payload) {
    auto *container = new QWidget;
    auto *layout = new QVBoxLayout(container);
    const QJsonObject accessibility =
        payload.value(QStringLiteral("accessibility")).toObject();
    const QString labelText =
        payload.value(QStringLiteral("label")).toString();
    applyAccessibility(container, accessibility);

    if (payload.value(QStringLiteral("purpose")).toString()
        == QStringLiteral("capture")) {
        auto *input = new QLineEdit;
        const QString binding =
            payload.value(QStringLiteral("id")).toString();
        input->setObjectName(binding);
        input->setPlaceholderText(labelText);
        applyAccessibility(input, accessibility);
        connect(input, &QLineEdit::editingFinished, this,
                [this, input, binding]() {
                    emit valueReady(
                        m_surfaceId, binding,
                        QJsonObject{{QStringLiteral("text"), input->text()}});
                });
        layout->addWidget(input);
        return container;
    }

    auto *image = new QLabel;
    image->setAlignment(Qt::AlignCenter);
    applyAccessibility(image, accessibility);
    layout->addWidget(image);

    const QJsonArray payloadValues =
        payload.value(QStringLiteral("payloads")).toArray();
    QStringList frames;
    frames.reserve(payloadValues.size());
    for (const auto &value : payloadValues) {
        frames.push_back(value.toString());
    }
    if (!frames.isEmpty()) {
        image->setPixmap(encodeQr(frames.first()));
    }
    if (frames.size() > 1) {
        auto *timer = new QTimer(container);
        timer->setInterval(frameIntervalMilliseconds);
        connect(timer, &QTimer::timeout, image,
                [image, frames, frame = 0]() mutable {
                    frame = (frame + 1) % frames.size();
                    image->setPixmap(encodeQr(frames.at(frame)));
                });
        timer->start();
    }
    if (!labelText.isEmpty()) {
        auto *label = new QLabel(labelText);
        label->setAlignment(Qt::AlignCenter);
        layout->addWidget(label);
    }
    return container;
}
