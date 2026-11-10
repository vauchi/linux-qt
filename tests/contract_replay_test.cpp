// SPDX-FileCopyrightText: 2026 Mattia Egloff <mattia.egloff@pm.me>
// SPDX-License-Identifier: GPL-3.0-or-later

//! The Qt presentation state replays the contract's fixture corpus with no
//! engine behind it (ADR-078 D-11): every command batch is accepted and
//! leaves exactly the state the corpus expects.
//!
//! Usage: contract_replay_test <presentation_contract_v1.json>
//!                             <screen_catalog_v1.json>

#include "coreui/presentationstate.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <cstdio>
#include <cstdlib>

namespace {

[[noreturn]] void fail(const QString &message) {
    std::fprintf(stderr, "contract_replay_test: %s\n", qPrintable(message));
    std::exit(1);
}

void require(bool condition, const QString &message) {
    if (!condition) fail(message);
}

QJsonObject corpus(const char *path) {
    QFile file(QString::fromLocal8Bit(path));
    require(file.open(QIODevice::ReadOnly),
            QStringLiteral("cannot read %1").arg(file.fileName()));
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll());
    require(document.isObject(),
            QStringLiteral("%1 is not a JSON object").arg(file.fileName()));
    const QJsonObject object = document.object();
    require(object.value("schema_version").toInt() == 1,
            QStringLiteral("%1 is not schema version 1").arg(file.fileName()));
    return object;
}

void applyAll(PresentationState &state, const QJsonValue &commands,
              const QString &where) {
    require(commands.isArray(),
            QStringLiteral("%1: commands are not a list").arg(where));
    for (const QJsonValue &command : commands.toArray()) {
        // A command without a payload is a bare string on the wire
        // (`"BleStopAdvertising"`); none of them is presentation state.
        if (command.isString() && !command.toString().isEmpty()) continue;
        require(command.isObject() && state.apply(command.toObject()),
                QStringLiteral("%1: command must be accepted: %2")
                    .arg(where,
                         QString::fromUtf8(QJsonDocument(command.toObject())
                                               .toJson(QJsonDocument::Compact)
                                               .left(200))));
    }
}

// @scenario: generic_presentation_protocol.feature :: Every shell renders the same prepared presentation
void thePresentationContractReachesTheExpectedState(const QJsonObject &fixture) {
    PresentationState state;
    applyAll(state, fixture.value("initial_commands"), "initial_commands");
    const QJsonArray steps = fixture.value("steps").toArray();
    require(!steps.isEmpty(), "the fixture ships steps");
    for (qsizetype index = 0; index < steps.size(); ++index) {
        applyAll(state, steps.at(index).toObject().value("commands"),
                 QStringLiteral("step %1").arg(index));
    }

    const QString active = state.activeSurfaceId();
    const auto surface = state.surface(active);
    require(surface.has_value(), "a surface is visible after the replay");
    const QJsonObject actual{
        {"active_surface_id", active},
        {"surface", *surface},
        {"context_bar", state.contextBar()},
    };
    const QJsonObject expected = fixture.value("expected_state").toObject();
    require(actual == expected,
            QStringLiteral("state differs from expected_state:\n%1\nexpected:\n%2")
                .arg(QString::fromUtf8(QJsonDocument(actual).toJson()),
                     QString::fromUtf8(QJsonDocument(expected).toJson())));
}

// @scenario: generic_presentation_protocol.feature :: Every shell renders the same prepared presentation
void everyCatalogScreenIsHeldAsSentFromAColdState(const QJsonObject &catalog) {
    const QJsonArray screens = catalog.value("screens").toArray();
    require(!screens.isEmpty(), "the catalog ships screens");

    for (const QJsonValue &entry : screens) {
        const QJsonObject screen = entry.toObject();
        const QString codeId = screen.value("code_id").toString();
        require(!codeId.isEmpty(), "a catalog screen has a code id");

        PresentationState state;
        applyAll(state, screen.value("commands"), codeId);

        int replaced = 0;
        for (const QJsonValue &command : screen.value("commands").toArray()) {
            const QJsonValue replace = command.toObject().value("ReplaceSurface");
            if (!replace.isObject()) continue;
            ++replaced;
            const QJsonObject sent = replace.toObject().value("surface").toObject();
            const QString surfaceId = sent.value("surface_id").toString();
            const auto held = state.surface(surfaceId);
            require(held.has_value() && *held == sent,
                    QStringLiteral("%1: surface %2 is held as sent")
                        .arg(codeId, surfaceId));
        }
        require(replaced > 0,
                QStringLiteral("%1: the screen shows a surface").arg(codeId));
        require(!state.contextBar().isEmpty(),
                QStringLiteral("%1: the shown surface has its context bar")
                    .arg(codeId));
    }
}

} // namespace

int main(int argc, char **argv) {
    if (argc != 3) {
        fail("usage: contract_replay_test <presentation_contract_v1.json> "
             "<screen_catalog_v1.json>");
    }
    thePresentationContractReachesTheExpectedState(corpus(argv[1]));
    everyCatalogScreenIsHeldAsSentFromAColdState(corpus(argv[2]));
    std::printf("contract_replay_test: OK\n");
    return 0;
}
