// SPDX-FileCopyrightText: 2026 Mattia Egloff <mattia.egloff@pm.me>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "coreui/navigationsidebar.h"
#include "coreui/presentationstate.h"

#include <QApplication>
#include <QJsonArray>
#include <QJsonObject>
#include <QToolButton>
#include <cassert>

namespace {

QJsonObject replaceSurface(const QString &id, int revision) {
    return {
        {"ReplaceSurface",
         QJsonObject{
             {"surface",
              QJsonObject{
                  {"surface_id", id},
                  {"revision", revision},
                  {"title", id},
                  {"layout", "scroll"},
                  {"nodes", QJsonArray{}},
              }}},
        },
    };
}

QJsonObject navigationItem(const QString &interactionId, const QString &label,
                           const QString &iconToken, bool selected,
                           int badgeCount) {
    return {
        {"interaction_id", interactionId},
        {"label", label},
        {"accessibility_label", label},
        {"icon_token", iconToken},
        {"selected", selected},
        {"badge_count", badgeCount},
    };
}

QJsonObject setNavigation(const QString &surfaceId, int revision,
                          const QJsonArray &items) {
    return {
        {"SetNavigation",
         QJsonObject{
             {"surface_id", surfaceId},
             {"revision", revision},
             {"navigation", QJsonObject{{"items", items}}},
         }},
    };
}

void testStateParsesFixtureShapedNavigation() {
    PresentationState state;
    assert(state.apply(replaceSurface("contacts", 1)));
    const QJsonArray items{navigationItem(
        "surface.1.context.presentation.navigation.contacts", "Contacts",
        "person.2", true, 0)};
    assert(state.apply(setNavigation("contacts", 1, items)));
    assert(state.navigation().value("items").toArray() == items);
}

void testStateEmptyNavigationList() {
    PresentationState state;
    assert(state.apply(replaceSurface("locked", 1)));
    assert(state.apply(setNavigation("locked", 1, QJsonArray{})));
    assert(state.navigation().value("items").toArray().isEmpty());
}

void testStateRejectsStaleNavigationRevision() {
    PresentationState state;
    assert(state.apply(replaceSurface("contacts", 3)));
    const QJsonArray items{
        navigationItem("nav.contacts", "Contacts", "person.2", true, 0)};
    assert(state.apply(setNavigation("contacts", 3, items)));
    assert(!state.apply(setNavigation("contacts", 2, QJsonArray{})));
    assert(state.navigation().value("items").toArray() == items);
}

void testStateClearsNavigationOnReplaceSurface() {
    PresentationState state;
    assert(state.apply(replaceSurface("contacts", 1)));
    const QJsonArray items{
        navigationItem("nav.contacts", "Contacts", "person.2", true, 0)};
    assert(state.apply(setNavigation("contacts", 1, items)));
    assert(state.apply(replaceSurface("contacts", 2)));
    assert(state.navigation().value("items").toArray().isEmpty());
}

void testNavigationRowsParsesSixFields() {
    const QJsonObject navigation{
        {"items",
         QJsonArray{navigationItem("nav.contacts", "Contacts", "person.2",
                                   true, 3)}}};
    const QList<vauchi::NavigationRow> rows =
        vauchi::navigationRows(navigation);
    assert(rows.size() == 1);
    assert(rows.at(0).interactionId == QStringLiteral("nav.contacts"));
    assert(rows.at(0).label == QStringLiteral("Contacts"));
    assert(rows.at(0).accessibilityLabel == QStringLiteral("Contacts"));
    assert(rows.at(0).iconToken == QStringLiteral("person.2"));
    assert(rows.at(0).selected);
    assert(rows.at(0).badgeCount == 3);
}

void testNavigationRowsEmptyListYieldsNoRows() {
    const QJsonObject navigation{{"items", QJsonArray{}}};
    assert(vauchi::navigationRows(navigation).isEmpty());
    assert(vauchi::selectedNavigationRow({}) == -1);
}

void testSelectedNavigationRowFindsIndex() {
    const QJsonObject navigation{
        {"items",
         QJsonArray{
             navigationItem("nav.contacts", "Contacts", "person.2", false, 0),
             navigationItem("nav.cards", "Cards", "folder", true, 0),
         }}};
    assert(vauchi::selectedNavigationRow(vauchi::navigationRows(navigation))
           == 1);
}

void testSidebarBuildsOneRowPerItemWithIconLabelAndAccessibleName() {
    const QJsonObject navigation{
        {"items",
         QJsonArray{
             navigationItem("nav.contacts", "Contacts", "person.2", true, 0),
             navigationItem("nav.cards", "Cards", "folder", false, 3),
         }}};
    NavigationSidebar sidebar(navigation);
    const QList<QToolButton *> rows = sidebar.findChildren<QToolButton *>(
        QStringLiteral("navigation-sidebar-row"));
    assert(rows.size() == 2);
    assert(rows.at(0)->isChecked());
    assert(rows.at(0)->accessibleName() == QStringLiteral("Contacts"));
    assert(!rows.at(0)->icon().isNull());
    assert(!rows.at(1)->isChecked());
    assert(rows.at(1)->text().contains(QStringLiteral("3")));
}

void testSidebarActivationEmitsInteractionId() {
    const QJsonObject navigation{
        {"items",
         QJsonArray{navigationItem("nav.contacts", "Contacts", "person.2",
                                   true, 0)}}};
    NavigationSidebar sidebar(navigation);
    QString activated;
    QObject::connect(&sidebar, &NavigationSidebar::interactionReady,
                     [&](const QString &interactionId) {
                         activated = interactionId;
                     });
    sidebar
        .findChildren<QToolButton *>(QStringLiteral("navigation-sidebar-row"))
        .first()
        ->click();
    assert(activated == QStringLiteral("nav.contacts"));
}

void testSidebarEmptyItemsYieldNoRows() {
    NavigationSidebar sidebar(QJsonObject{{"items", QJsonArray{}}});
    assert(sidebar
               .findChildren<QToolButton *>(
                   QStringLiteral("navigation-sidebar-row"))
               .isEmpty());
}

} // namespace

int main(int argc, char **argv) {
    QApplication app(argc, argv);

    testStateParsesFixtureShapedNavigation();
    testStateEmptyNavigationList();
    testStateRejectsStaleNavigationRevision();
    testStateClearsNavigationOnReplaceSurface();
    testNavigationRowsParsesSixFields();
    testNavigationRowsEmptyListYieldsNoRows();
    testSelectedNavigationRowFindsIndex();
    testSidebarBuildsOneRowPerItemWithIconLabelAndAccessibleName();
    testSidebarActivationEmitsInteractionId();
    testSidebarEmptyItemsYieldNoRows();
    return 0;
}
