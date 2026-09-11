// SPDX-FileCopyrightText: 2026 Mattia Egloff <mattia.egloff@pm.me>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "screencatalog.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSet>

namespace vauchi {

namespace {

constexpr int kSupportedSchemaVersion = 1;

bool isSafeFileStem(const QString &codeId) {
    static const QRegularExpression safeStem(
        QStringLiteral("^[A-Za-z0-9][A-Za-z0-9_.-]{0,63}$"));
    return safeStem.match(codeId).hasMatch();
}

QString replaceSurfaceTarget(const QJsonArray &commands) {
    for (const auto &command : commands) {
        const QJsonObject replace =
            command.toObject().value(QStringLiteral("ReplaceSurface")).toObject();
        if (!replace.isEmpty()) {
            return replace.value(QStringLiteral("surface"))
                .toObject()
                .value(QStringLiteral("surface_id"))
                .toString();
        }
    }
    return {};
}

QList<ScreenCatalogEntry> entriesFromContract(const QJsonObject &contract) {
    QList<ScreenCatalogEntry> entries;
    const QJsonArray initial =
        contract.value(QStringLiteral("initial_commands")).toArray();
    const QString initialSurface = replaceSurfaceTarget(initial);
    if (!initialSurface.isEmpty()) {
        entries.append({initialSurface, initialSurface, QStringLiteral("en"),
                        initial});
    }
    const QJsonArray steps = contract.value(QStringLiteral("steps")).toArray();
    for (int index = 0; index < steps.size(); ++index) {
        const QJsonArray commands =
            steps.at(index).toObject().value(QStringLiteral("commands")).toArray();
        const QString surface = replaceSurfaceTarget(commands);
        if (surface.isEmpty()) {
            continue;
        }
        const QString codeId =
            QStringLiteral("%1-step%2").arg(surface).arg(index + 1);
        entries.append({codeId, surface, QStringLiteral("en"), commands});
    }
    return entries;
}

QList<ScreenCatalogEntry> entriesFromScreens(const QJsonArray &screens) {
    QList<ScreenCatalogEntry> entries;
    for (const auto &value : screens) {
        const QJsonObject screen = value.toObject();
        entries.append({screen.value(QStringLiteral("code_id")).toString(),
                        screen.value(QStringLiteral("title")).toString(),
                        screen.value(QStringLiteral("locale")).toString(),
                        screen.value(QStringLiteral("commands")).toArray()});
    }
    return entries;
}

ScreenCatalog rejected(const QString &error) {
    ScreenCatalog catalog;
    catalog.error = error;
    return catalog;
}

} // namespace

ScreenCatalog parseScreenCatalog(const QByteArray &json) {
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(json, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        return rejected(QStringLiteral("catalog is not a JSON object: %1")
                            .arg(parseError.errorString()));
    }
    const QJsonObject root = document.object();
    const int schemaVersion =
        root.value(QStringLiteral("schema_version")).toInt(-1);
    if (schemaVersion != kSupportedSchemaVersion) {
        return rejected(QStringLiteral("unsupported schema_version %1 (want %2)")
                            .arg(schemaVersion)
                            .arg(kSupportedSchemaVersion));
    }

    ScreenCatalog catalog;
    if (root.contains(QStringLiteral("screens"))) {
        catalog.screens =
            entriesFromScreens(root.value(QStringLiteral("screens")).toArray());
    } else if (root.contains(QStringLiteral("initial_commands"))) {
        catalog.screens = entriesFromContract(root);
    } else {
        return rejected(QStringLiteral(
            "catalog has neither \"screens\" nor \"initial_commands\""));
    }
    if (catalog.screens.isEmpty()) {
        return rejected(QStringLiteral("catalog lists no screens"));
    }

    QSet<QString> seen;
    for (const auto &entry : catalog.screens) {
        if (!isSafeFileStem(entry.codeId)) {
            return rejected(
                QStringLiteral("code_id %1 is not a safe file-name stem")
                    .arg(QString::fromUtf8(
                        QJsonDocument(QJsonArray{entry.codeId}).toJson(
                            QJsonDocument::Compact))));
        }
        if (seen.contains(entry.codeId)) {
            return rejected(
                QStringLiteral("duplicate code_id %1").arg(entry.codeId));
        }
        seen.insert(entry.codeId);
        if (entry.commands.isEmpty()) {
            return rejected(
                QStringLiteral("screen %1 has no commands").arg(entry.codeId));
        }
    }
    return catalog;
}

QString screenCatalogFileName(const QString &codeId, const QString &variant) {
    if (variant.isEmpty()) {
        return codeId + QStringLiteral(".png");
    }
    return codeId + QLatin1Char('.') + variant + QStringLiteral(".png");
}

} // namespace vauchi
