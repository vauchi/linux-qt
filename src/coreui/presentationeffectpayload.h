// SPDX-FileCopyrightText: 2026 Mattia Egloff <mattia.egloff@pm.me>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QByteArray>
#include <QJsonObject>
#include <QString>
#include <optional>

struct ExportFilePayload {
    QString suggestedName;
    QString mimeType;
    QByteArray data;
};

class PresentationEffectPayload {
public:
    static std::optional<ExportFilePayload>
    exportFile(const QJsonObject &command);
};
