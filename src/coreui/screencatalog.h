// SPDX-FileCopyrightText: 2026 Mattia Egloff <mattia.egloff@pm.me>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QJsonArray>
#include <QList>
#include <QString>

namespace vauchi {

/// One screen of Core's screen catalog (`vauchi-app/fixtures/
/// screen_catalog_v1.json`): the command batch that, replayed through the
/// production reducer, presents that screen without any navigation.
struct ScreenCatalogEntry {
    QString codeId;
    QString title;
    QString locale;
    QJsonArray commands;
};

struct ScreenCatalog {
    QList<ScreenCatalogEntry> screens;
    /// Non-empty when the document was rejected; `screens` is then empty.
    QString error;
    bool ok() const { return error.isEmpty(); }
};

/// Parses a catalog document. Accepts Core's `{"schema_version": 1,
/// "screens": [...]}` shape, and — as the fallback CI uses until that
/// fixture exists on Core main — the presentation-contract fixture shape
/// (`initial_commands` + `steps[].commands`), naming each batch after its
/// ReplaceSurface target (`<surface_id>` for the initial batch,
/// `<surface_id>-step<n>` per step; a step without a ReplaceSurface is
/// skipped). A `code_id` is the file-name stem the PNG is written under,
/// so anything outside `[A-Za-z0-9_.-]`, a leading dot, or a duplicate is
/// rejected as a whole-document error rather than sanitised.
ScreenCatalog parseScreenCatalog(const QByteArray &json);

/// `<code_id>.png` for the default variant, `<code_id>.<variant>.png`
/// otherwise.
QString screenCatalogFileName(const QString &codeId, const QString &variant);

} // namespace vauchi
