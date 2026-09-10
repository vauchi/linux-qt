// SPDX-FileCopyrightText: 2026 Mattia Egloff <mattia.egloff@pm.me>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "presentationsurface_avatar.h"

#include <QFont>
#include <QPainter>
#include <QPainterPath>
#include <algorithm>

namespace vauchi {

QPixmap avatarPixmap(const QPixmap &imageData, const QString &fallbackText,
                     bool circular, int diameter, int cornerRadius,
                     const QColor &fillColor, const QColor &textColor) {
    if (diameter <= 0) {
        return QPixmap();
    }

    QPixmap canvas(diameter, diameter);
    canvas.fill(Qt::transparent);

    QPainter painter(&canvas);
    painter.setRenderHint(QPainter::Antialiasing);
    QPainterPath clip;
    if (circular) {
        clip.addEllipse(0, 0, diameter, diameter);
    } else {
        clip.addRoundedRect(0, 0, diameter, diameter, cornerRadius,
                            cornerRadius);
    }
    painter.setClipPath(clip);

    if (!imageData.isNull()) {
        const QPixmap scaled = imageData.scaled(
            diameter, diameter, Qt::KeepAspectRatioByExpanding,
            Qt::SmoothTransformation);
        painter.drawPixmap((diameter - scaled.width()) / 2,
                           (diameter - scaled.height()) / 2, scaled);
        return canvas;
    }

    painter.fillPath(clip, fillColor);
    if (!fallbackText.isEmpty()) {
        QFont font = painter.font();
        font.setBold(true);
        font.setPixelSize(std::max(1, diameter / 3));
        painter.setFont(font);
        painter.setPen(textColor);
        painter.drawText(canvas.rect(), Qt::AlignCenter, fallbackText);
    }
    return canvas;
}

} // namespace vauchi
