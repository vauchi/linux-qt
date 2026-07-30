// SPDX-FileCopyrightText: 2026 Mattia Egloff <mattia.egloff@pm.me>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "presentationeffectpayload.h"

#include <QJsonArray>

std::optional<ExportFilePayload>
PresentationEffectPayload::exportFile(const QJsonObject &command) {
    const QJsonValue exportValue =
        command.value(QStringLiteral("ExportFile"));
    if (!exportValue.isObject()) {
        return std::nullopt;
    }
    const QJsonValue fileValue =
        exportValue.toObject().value(QStringLiteral("file"));
    if (!fileValue.isObject()) {
        return std::nullopt;
    }
    const QJsonObject file = fileValue.toObject();
    const QJsonValue name = file.value(QStringLiteral("suggested_name"));
    const QJsonValue mimeType = file.value(QStringLiteral("mime_type"));
    const QJsonValue data = file.value(QStringLiteral("data"));
    if (!name.isString() || !mimeType.isString() || !data.isArray()) {
        return std::nullopt;
    }

    QByteArray bytes;
    const QJsonArray values = data.toArray();
    bytes.reserve(values.size());
    for (const auto &value : values) {
        const int byte = value.toInt(-1);
        if (!value.isDouble() || byte < 0 || byte > 255) {
            return std::nullopt;
        }
        bytes.append(static_cast<char>(byte));
    }
    return ExportFilePayload{
        name.toString(),
        mimeType.toString(),
        bytes,
    };
}
