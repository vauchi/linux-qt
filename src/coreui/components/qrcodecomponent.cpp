// SPDX-FileCopyrightText: 2026 Mattia Egloff <mattia.egloff@pm.me>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "qrcodecomponent.h"
#include "../thememanager.h"
#include <QLabel>
#include <QVBoxLayout>
#include <QPainter>
#include <QPixmap>
#include <qrencode.h>

static constexpr int QR_SIZE = 200;

QWidget *QrcodeComponent::render(const QJsonObject &data) {
    auto *container = new QWidget;
    container->setObjectName(data["id"].toString());
    auto *layout = new QVBoxLayout(container);
    layout->setContentsMargins(0, 0, 0, 0);

    QString qrData = data["data"].toString();
    QString mode = data["mode"].toString();
    QString labelText = data["label"].toString();

    container->setAccessibleName(labelText);
    if (data.contains("a11y") && data["a11y"].isObject()) {
        auto a11y = data["a11y"].toObject();
        auto a11yLabel = a11y.value("label").toString();
        if (!a11yLabel.isEmpty()) {
            container->setAccessibleName(a11yLabel);
        }
        auto hint = a11y.value("hint").toString();
        if (!hint.isEmpty()) {
            container->setAccessibleDescription(hint);
        }
    }

    if (mode == "Scan") {
        auto *label = new QLabel(labelText);
        label->setAlignment(Qt::AlignCenter);
        layout->addWidget(label);
        return container;
    }

    auto addRenderWarning = [&]() {
        auto *warning = new QLabel(QStringLiteral("⚠"));
        warning->setAlignment(Qt::AlignCenter);
        warning->setAccessibleName(container->accessibleName());
        layout->addWidget(warning);
    };

    if (qrData.isEmpty()) {
        addRenderWarning();
        return container;
    }

    QByteArray utf8 = qrData.toUtf8();
    QRcode *code = QRcode_encodeString(utf8.constData(), 0, QR_ECLEVEL_M, QR_MODE_8, 1);
    if (!code) {
        addRenderWarning();
        return container;
    }

    int moduleCount = code->width;
    if (moduleCount <= 0) {
        QRcode_free(code);
        addRenderWarning();
        return container;
    }
    int scale = QR_SIZE / moduleCount;
    if (scale < 1) scale = 1;
    int imgSize = moduleCount * scale;

    QPixmap pixmap(imgSize, imgSize);
    pixmap.fill(Qt::white);
    QPainter painter(&pixmap);
    painter.setPen(Qt::NoPen);
    painter.setBrush(Qt::black);

    for (int y = 0; y < moduleCount; ++y) {
        for (int x = 0; x < moduleCount; ++x) {
            if (code->data[y * moduleCount + x] & 1) {
                painter.drawRect(x * scale, y * scale, scale, scale);
            }
        }
    }
    painter.end();
    QRcode_free(code);

    auto *qrLabel = new QLabel;
    qrLabel->setPixmap(pixmap.scaled(QR_SIZE, QR_SIZE, Qt::KeepAspectRatio, Qt::FastTransformation));
    qrLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(qrLabel);

    // Optional label below QR
    if (!labelText.isEmpty()) {
        auto *textLabel = new QLabel(labelText);
        textLabel->setAlignment(Qt::AlignCenter);
        textLabel->setStyleSheet(ThemeManager::fontFamilyCss() +
                                 ThemeManager::styleForRole(ThemeRole::SecondaryText) +
                                 QStringLiteral(" font-size: 12px;"));
        layout->addWidget(textLabel);
    }

    return container;
}
