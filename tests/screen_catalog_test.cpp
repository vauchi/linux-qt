// SPDX-FileCopyrightText: 2026 Mattia Egloff <mattia.egloff@pm.me>
// SPDX-License-Identifier: GPL-3.0-or-later

//! Screen-catalog document parsing for `qvauchi --render-catalog`: Core's
//! `screen_catalog_v1.json` shape, the presentation-contract fallback shape,
//! rejection of unsafe file-name stems, and the per-variant output names.

#include "coreui/screencatalog.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <cassert>

namespace {

QJsonObject replaceSurface(const QString &surfaceId) {
    return {
        {"ReplaceSurface",
         QJsonObject{{"surface",
                      QJsonObject{{"surface_id", surfaceId},
                                  {"revision", 1},
                                  {"title", surfaceId},
                                  {"layout", "scroll"},
                                  {"nodes", QJsonArray{}}}}}},
    };
}

QJsonObject screen(const QString &codeId, const QJsonArray &commands) {
    return {
        {"code_id", codeId},
        {"title", codeId},
        {"locale", "en"},
        {"commands", commands},
    };
}

QByteArray catalogDocument(const QJsonArray &screens, int schemaVersion = 1) {
    return QJsonDocument(QJsonObject{{"schema_version", schemaVersion},
                                     {"screens", screens}})
        .toJson();
}

void testParsesCatalogEntriesInOrder() {
    const QJsonArray contacts{replaceSurface("contacts")};
    const QJsonArray settings{replaceSurface("settings")};
    const auto catalog = vauchi::parseScreenCatalog(catalogDocument(
        QJsonArray{screen("contacts", contacts), screen("settings", settings)}));
    assert(catalog.ok());
    assert(catalog.screens.size() == 2);
    assert(catalog.screens.at(0).codeId == QStringLiteral("contacts"));
    assert(catalog.screens.at(0).title == QStringLiteral("contacts"));
    assert(catalog.screens.at(0).locale == QStringLiteral("en"));
    assert(catalog.screens.at(0).commands == contacts);
    assert(catalog.screens.at(1).codeId == QStringLiteral("settings"));
    assert(catalog.screens.at(1).commands == settings);
}

void testRejectsUnknownSchemaVersion() {
    const auto catalog = vauchi::parseScreenCatalog(catalogDocument(
        QJsonArray{screen("contacts", QJsonArray{replaceSurface("contacts")})},
        2));
    assert(!catalog.ok());
    assert(catalog.screens.isEmpty());
    assert(catalog.error.contains(QStringLiteral("schema_version")));
}

void testRejectsMalformedJson() {
    const auto catalog = vauchi::parseScreenCatalog("{not json");
    assert(!catalog.ok());
    assert(catalog.screens.isEmpty());
}

void testRejectsEmptyCatalog() {
    const auto catalog =
        vauchi::parseScreenCatalog(catalogDocument(QJsonArray{}));
    assert(!catalog.ok());
}

void testRejectsEntryWithoutCommands() {
    const auto catalog = vauchi::parseScreenCatalog(
        catalogDocument(QJsonArray{screen("contacts", QJsonArray{})}));
    assert(!catalog.ok());
    assert(catalog.error.contains(QStringLiteral("contacts")));
}

// code_id becomes a file-name stem under the output directory, so a value
// that could escape it or hide the file is refused rather than sanitised.
void testRejectsUnsafeCodeIds() {
    const QJsonArray commands{replaceSurface("contacts")};
    for (const QString &unsafe :
         {QStringLiteral(""), QStringLiteral("../escape"),
          QStringLiteral("a/b"), QStringLiteral(".hidden"),
          QStringLiteral(".."), QStringLiteral("with space"),
          QStringLiteral("nul\0byte")}) {
        const auto catalog = vauchi::parseScreenCatalog(
            catalogDocument(QJsonArray{screen(unsafe, commands)}));
        assert(!catalog.ok());
        assert(catalog.screens.isEmpty());
    }
    const auto catalog = vauchi::parseScreenCatalog(catalogDocument(
        QJsonArray{screen("contact-detail.v2_Locked", commands)}));
    assert(catalog.ok());
}

void testRejectsDuplicateCodeIds() {
    const QJsonArray commands{replaceSurface("contacts")};
    const auto catalog = vauchi::parseScreenCatalog(catalogDocument(
        QJsonArray{screen("contacts", commands), screen("contacts", commands)}));
    assert(!catalog.ok());
    assert(catalog.error.contains(QStringLiteral("duplicate")));
}

// Until Core's catalog fixture lands, CI renders the presentation contract
// fixture instead: its initial batch plus one entry per step, named after
// the batch's ReplaceSurface target with the step index for uniqueness.
void testConvertsPresentationContractFixture() {
    const QJsonArray initial{replaceSurface("onboarding")};
    const QJsonArray step1{replaceSurface("onboarding")};
    const QJsonArray step2{replaceSurface("contacts")};
    const QByteArray contract =
        QJsonDocument(
            QJsonObject{
                {"schema_version", 1},
                {"initial_commands", initial},
                {"steps",
                 QJsonArray{QJsonObject{{"event", QJsonObject{}},
                                        {"commands", step1}},
                            QJsonObject{{"event", QJsonObject{}},
                                        {"commands", step2}}}},
            })
            .toJson();
    const auto catalog = vauchi::parseScreenCatalog(contract);
    assert(catalog.ok());
    assert(catalog.screens.size() == 3);
    assert(catalog.screens.at(0).codeId == QStringLiteral("onboarding"));
    assert(catalog.screens.at(0).commands == initial);
    assert(catalog.screens.at(1).codeId
           == QStringLiteral("onboarding-step1"));
    assert(catalog.screens.at(1).commands == step1);
    assert(catalog.screens.at(2).codeId == QStringLiteral("contacts-step2"));
    assert(catalog.screens.at(2).commands == step2);
}

void testContractStepWithoutSurfaceIsSkipped() {
    const QByteArray contract =
        QJsonDocument(
            QJsonObject{
                {"schema_version", 1},
                {"initial_commands", QJsonArray{replaceSurface("onboarding")}},
                {"steps",
                 QJsonArray{QJsonObject{
                     {"event", QJsonObject{}},
                     {"commands",
                      QJsonArray{QJsonObject{
                          {"ScheduleWakeup", QJsonObject{{"seconds", 5}}}}}}}}},
            })
            .toJson();
    const auto catalog = vauchi::parseScreenCatalog(contract);
    assert(catalog.ok());
    assert(catalog.screens.size() == 1);
}

void testOutputFileNames() {
    assert(vauchi::screenCatalogFileName("contacts", QString())
           == QStringLiteral("contacts.png"));
    assert(vauchi::screenCatalogFileName("contacts", "light")
           == QStringLiteral("contacts.light.png"));
    assert(vauchi::screenCatalogFileName("contacts", "large")
           == QStringLiteral("contacts.large.png"));
}

} // namespace

int main() {
    testParsesCatalogEntriesInOrder();
    testRejectsUnknownSchemaVersion();
    testRejectsMalformedJson();
    testRejectsEmptyCatalog();
    testRejectsEntryWithoutCommands();
    testRejectsUnsafeCodeIds();
    testRejectsDuplicateCodeIds();
    testConvertsPresentationContractFixture();
    testContractStepWithoutSurfaceIsSkipped();
    testOutputFileNames();
    return 0;
}
