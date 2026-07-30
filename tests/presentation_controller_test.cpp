// SPDX-FileCopyrightText: 2026 Mattia Egloff <mattia.egloff@pm.me>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "coreui/presentationcontroller.h"
#include "coreui/presentationeffectpayload.h"

#include <QApplication>
#include <QDialog>
#include <QJsonArray>
#include <QLineEdit>
#include <QMenu>
#include <QPropertyAnimation>
#include <QPushButton>
#include <cassert>

namespace {

QJsonObject action(const QString &id, const QString &label,
                   const QJsonValue &shortcut = QJsonValue::Null) {
    return {
        {"interaction_id", id},
        {"label", label},
        {"accessibility_label", label},
        {"icon_token", QJsonValue::Null},
        {"enabled", true},
        {"shortcut", shortcut},
    };
}

QJsonArray baseCommands() {
    const QJsonObject accessibility{
        {"label", "Name"},
        {"description", QJsonValue::Null},
    };
    const QJsonObject surface{
        {"surface_id", "settings"},
        {"revision", 1},
        {"title", "Settings"},
        {"subtitle", QJsonValue::Null},
        {"accessibility_label", "Settings"},
        {"layout", "fixed"},
        {"tokens", QJsonObject{}},
        {"nodes",
         QJsonArray{
             QJsonObject{
                 {"Input",
                  QJsonObject{{"binding_id", "name"},
                              {"label", "Name"},
                              {"value", "Alice"},
                              {"placeholder", QJsonValue::Null},
                              {"input_kind", "text"},
                              {"max_length", QJsonValue::Null},
                              {"validation_error", QJsonValue::Null},
                              {"enabled", true},
                              {"accessibility", accessibility}}}},
         }},
    };
    QJsonObject primary = action("undo", "Undo", "undo");
    primary.insert(QStringLiteral("tone"), QStringLiteral("destructive"));
    const QJsonObject bar{
        {"back", action("back", "Back", "back")},
        {"navigation", action("navigation", "Navigate")},
        {"primary", primary},
        {"secondary", action("secondary", "More")},
    };
    const QJsonObject profile{
        {"window_class", "compact"},
        {"pane_layout", "single"},
        {"primary_surface", "settings"},
        {"detail_surface", QJsonValue::Null},
        {"active_surface", "settings"},
    };
    return {
        QJsonObject{{"ReplaceSurface",
                     QJsonObject{{"surface", surface}}}},
        QJsonObject{{"SetContextBar",
                     QJsonObject{{"surface_id", "settings"},
                                 {"revision", 1},
                                 {"bar", bar}}}},
        QJsonObject{{"SetPresentationProfile",
                     QJsonObject{{"profile", profile}}}},
    };
}

QJsonObject overlay(const QString &kind) {
    return {
        {"PresentOverlay",
         QJsonObject{
             {"surface_id", "settings"},
             {"revision", 1},
             {"overlay",
              QJsonObject{
                  {"kind", kind},
                  {"title", kind == QStringLiteral("navigation")
                                ? "Navigate"
                                : "More"},
                  {"items", QJsonArray{action("item", "Item")}},
              }},
         }},
    };
}

void assertNativeBarAndFocusRestoration() {
    PresentationController controller(nullptr);
    controller.resize(700, 600);
    controller.show();
    controller.dispatchCommands(baseCommands());
    QApplication::processEvents();

    auto *input = controller.findChild<QLineEdit *>("name");
    assert(input != nullptr);
    input->setFocus(Qt::OtherFocusReason);
    QApplication::processEvents();
    assert(QApplication::focusWidget() == input);

    controller.dispatchCommands(
        QJsonArray{baseCommands().at(2)});
    QApplication::processEvents();
    auto *restored = controller.findChild<QLineEdit *>("name");
    assert(restored != nullptr);
    assert(restored != input);
    assert(QApplication::focusWidget() == restored);

    auto *primary =
        controller.findChild<QPushButton *>("context-primary");
    auto *navigation =
        controller.findChild<QPushButton *>("context-navigation");
    auto *secondary =
        controller.findChild<QPushButton *>("context-secondary");
    assert(primary != nullptr);
    assert(primary->sizePolicy().horizontalPolicy()
           == QSizePolicy::Expanding);
    assert(primary->shortcut() == QKeySequence::Undo);
    assert(primary->property("tone").toString()
           == QStringLiteral("destructive"));
    assert(navigation != nullptr);
    assert(navigation->shortcut()
           == QKeySequence(QStringLiteral("Ctrl+K")));
    assert(secondary != nullptr);
    assert(secondary->shortcut()
           == QKeySequence(QStringLiteral("Alt+Down")));
}

void assertOverlayStructures(bool reducedMotion) {
    if (reducedMotion) {
        qputenv("QT_REDUCE_MOTION", "1");
    } else {
        qunsetenv("QT_REDUCE_MOTION");
    }
    PresentationController controller(nullptr);
    controller.resize(700, 600);
    controller.show();
    controller.dispatchCommands(baseCommands());

    controller.dispatchCommands(
        QJsonArray{overlay(QStringLiteral("navigation"))});
    QApplication::processEvents();
    auto *navigation = controller.findChild<QDialog *>();
    assert(navigation != nullptr);
    assert((navigation->findChild<QPropertyAnimation *>() == nullptr)
           == reducedMotion);
    navigation->close();
    QApplication::processEvents();

    controller.dispatchCommands(
        QJsonArray{overlay(QStringLiteral("action_menu"))});
    QApplication::processEvents();
    auto *actions = controller.findChild<QMenu *>();
    assert(actions != nullptr);
    assert(actions->findChild<QPropertyAnimation *>() == nullptr);
    actions->hide();
    QApplication::processEvents();
}

void assertExportPayloadUsesCanonicalSchema() {
    const QJsonObject command{
        {"ExportFile",
         QJsonObject{
             {"file",
              QJsonObject{
                  {"suggested_name", "contacts.vcf"},
                  {"mime_type", "text/vcard"},
                  {"data", QJsonArray{0, 127, 255}},
              }},
         }},
    };
    const auto payload = PresentationEffectPayload::exportFile(command);
    assert(payload.has_value());
    assert(payload->suggestedName == QStringLiteral("contacts.vcf"));
    assert(payload->mimeType == QStringLiteral("text/vcard"));
    assert(payload->data == QByteArray::fromRawData("\x00\x7f\xff", 3));

    const auto obsolete = PresentationEffectPayload::exportFile(
        QJsonObject{
            {"ExportFile",
             QJsonObject{{"file",
                          QJsonObject{{"suggested_filename", "old"},
                                      {"bytes", QJsonArray{1}}}}}},
        });
    assert(!obsolete.has_value());
}

} // namespace

int main(int argc, char **argv) {
    QApplication app(argc, argv);
    assertExportPayloadUsesCanonicalSchema();
    assertNativeBarAndFocusRestoration();
    assertOverlayStructures(false);
    assertOverlayStructures(true);
    qunsetenv("QT_REDUCE_MOTION");
    return 0;
}
