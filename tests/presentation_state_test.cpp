// SPDX-FileCopyrightText: 2026 Mattia Egloff <mattia.egloff@pm.me>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "coreui/presentationstate.h"
#include "vauchi.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <cassert>

static QJsonObject surface(const QString &id, int revision) {
    return {
        {"surface_id", id},
        {"revision", revision},
        {"title", id},
        {"layout", "scroll"},
        {"nodes", QJsonArray{}},
    };
}

static QJsonObject replaceSurface(const QString &id, int revision) {
    return {
        {"ReplaceSurface", QJsonObject{{"surface", surface(id, revision)}}},
    };
}

static QJsonObject setBar(const QString &id, int revision,
                          const QString &primary) {
    return {
        {"SetContextBar",
         QJsonObject{
             {"surface_id", id},
             {"revision", revision},
             {"bar",
              QJsonObject{
                  {"primary",
                   QJsonObject{{"interaction_id", primary},
                               {"label", primary},
                               {"accessibility_label", primary},
                               {"enabled", true}}},
              }},
         }},
    };
}

static QJsonObject profile(const QString &layout, const QString &active) {
    return {
        {"SetPresentationProfile",
         QJsonObject{
             {"profile",
              QJsonObject{
                  {"window_class",
                   layout == QStringLiteral("split") ? "expanded" : "compact"},
                  {"pane_layout", layout},
                  {"primary_surface", "contacts"},
                  {"detail_surface", "contact_detail"},
                  {"active_surface", active},
              }},
         }},
    };
}

int main() {
    // @scenario: generic_presentation_protocol.feature :: Every shell renders the same prepared presentation
    char *fixtureJson = vauchi_presentation_contract_fixture();
    assert(fixtureJson != nullptr);
    const QJsonObject fixture =
        QJsonDocument::fromJson(fixtureJson).object();
    vauchi_string_free(fixtureJson);
    assert(fixture.value("schema_version").toInt() == 1);

    PresentationState contractState;
    const auto applyBatch = [&contractState](const QJsonArray &commands) {
        for (const QJsonValue &command : commands) {
            assert(contractState.apply(command.toObject()));
        }
    };
    applyBatch(fixture.value("initial_commands").toArray());
    for (const QJsonValue &step : fixture.value("steps").toArray()) {
        applyBatch(step.toObject().value("commands").toArray());
    }

    const QJsonObject expected = fixture.value("expected_state").toObject();
    const QString active = expected.value("active_surface_id").toString();
    assert(contractState.activeSurfaceId() == active);
    assert(contractState.surface(active) == expected.value("surface").toObject());
    assert(contractState.contextBar()
           == expected.value("context_bar").toObject());

    PresentationState initial;
    assert(initial.apply(replaceSurface("onboarding", 1)));
    assert(initial.apply(setBar("onboarding", 1, "continue")));
    assert(initial.activeSurfaceId() == QStringLiteral("onboarding"));
    assert(initial.contextBar().value("primary").toObject()
               .value("interaction_id").toString()
           == QStringLiteral("continue"));

    PresentationState state;
    assert(state.apply(replaceSurface("contacts", 7)));
    assert(state.apply(replaceSurface("contact_detail", 7)));
    assert(state.apply(setBar("contacts", 7, "add_contact")));
    assert(state.apply(setBar("contact_detail", 7, "edit")));
    assert(state.apply(profile("split", "contact_detail")));

    assert(state.visibleSurfaceIds()
           == QStringList({"contacts", "contact_detail"}));
    assert(state.contextBar().value("primary").toObject()
               .value("interaction_id").toString()
           == QStringLiteral("edit"));

    assert(state.apply(profile("single", "contact_detail")));
    assert(state.visibleSurfaceIds() == QStringList({"contact_detail"}));
    assert(state.surface("contacts").has_value());

    assert(state.apply(profile("split", "contacts")));
    assert(state.visibleSurfaceIds()
           == QStringList({"contacts", "contact_detail"}));
    assert(state.contextBar().value("primary").toObject()
               .value("interaction_id").toString()
           == QStringLiteral("add_contact"));

    assert(!state.apply(setBar("contacts", 6, "stale")));
    assert(state.contextBar().value("primary").toObject()
               .value("interaction_id").toString()
           == QStringLiteral("add_contact"));

    // Core's revision advances only on user actions, so racing full
    // rebuilds (wakeup re-load, invalidation dispatch) legitimately
    // re-emit the same surface at the same revision. Only a strictly older
    // revision is stale.
    //
    // This shell is already correct. The check exists because two others
    // were not: Android and macOS both rejected an equal revision
    // (vauchi/android!610, vauchi/macos!346), and on Android it failed
    // every cold launch. Nothing pinned it here, so tightening `<` to
    // `<=` would reintroduce it silently.
    PresentationState revisions;
    assert(revisions.apply(replaceSurface("contacts", 2)));
    assert(revisions.apply(replaceSurface("contacts", 2)));
    assert(revisions.surface("contacts").has_value());
    assert(revisions.surface("contacts")->value("revision").toInteger() == 2);

    assert(!revisions.apply(replaceSurface("contacts", 1)));
    assert(revisions.surface("contacts")->value("revision").toInteger() == 2);
    return 0;
}
