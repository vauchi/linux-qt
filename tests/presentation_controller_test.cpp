// SPDX-FileCopyrightText: 2026 Mattia Egloff <mattia.egloff@pm.me>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "coreui/presentationcontroller.h"
#include "coreui/presentationeffectpayload.h"
#include "coreui/qrpasteprompt.h"

#include <QApplication>
#include <QDialog>
#include <QInputDialog>
#include <QJsonArray>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QPropertyAnimation>
#include <QPushButton>
#include <QVector>
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
    QApplication::setActiveWindow(&controller);
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

    // Design 2026-10-06 (vauchi/private#534): the separate row above the
    // tab bar/sidebar is retired. Back, navigation and secondary move into
    // the surface's own title row; primary moves to the bottom of the
    // surface content, not that row.
    assert(controller.findChild<QWidget *>("contextual-command-bar")
           == nullptr);
    auto *titleRow = controller.findChild<QWidget *>("surface-title-row");
    assert(titleRow != nullptr);
    auto *back = controller.findChild<QPushButton *>("context-back");
    assert(back != nullptr);
    assert(back->parentWidget() == titleRow);
    assert(back->shortcut() == QKeySequence::Back);
    assert(!back->icon().isNull());
    auto *screenTitle = controller.findChild<QLabel *>("screen_title");
    assert(screenTitle != nullptr);
    assert(screenTitle->parentWidget() == titleRow);
    assert(navigation->parentWidget() == titleRow);
    assert(secondary->parentWidget() == titleRow);
    assert(primary->parentWidget() != titleRow);
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

void assertNavigationItemsRespectMinimumTargetSize() {
    PresentationController controller(nullptr);
    controller.resize(700, 600);
    controller.show();

    // baseCommands() ships an empty `tokens` object; swap in real values so
    // the navigation overlay — built from the surface's tokens via
    // PresentationController::m_state, not from the overlay payload itself
    // — has something to read.
    QJsonArray commands = baseCommands();
    QJsonObject replace = commands.at(0).toObject();
    QJsonObject effect = replace.value(QStringLiteral("ReplaceSurface")).toObject();
    QJsonObject surface = effect.value(QStringLiteral("surface")).toObject();
    surface.insert(QStringLiteral("tokens"),
                   QJsonObject{{QStringLiteral("minimum_target_size"), 56},
                               {QStringLiteral("corner_radius"), 10}});
    effect.insert(QStringLiteral("surface"), surface);
    replace.insert(QStringLiteral("ReplaceSurface"), effect);
    commands[0] = replace;

    controller.dispatchCommands(commands);
    controller.dispatchCommands(
        QJsonArray{overlay(QStringLiteral("navigation"))});
    QApplication::processEvents();

    auto *navigation = controller.findChild<QDialog *>();
    assert(navigation != nullptr);
    auto *item = navigation->findChild<QPushButton *>();
    assert(item != nullptr);
    assert(item->minimumHeight() == 56);
    assert(item->styleSheet().contains(QStringLiteral("border-radius: 10px")));
    navigation->close();
    QApplication::processEvents();
}

// The sidebar lists the same destinations the navigation launcher opens,
// so beside a sidebar the bar leaves that control out; Core's fifth slot,
// info, is drawn after the others (vauchi/private#479).
void assertNoNavigationButtonBesideTheSidebarAndAnInfoButton() {
    PresentationController controller(nullptr);
    controller.resize(700, 600);
    controller.show();
    QJsonArray commands = baseCommands();
    QJsonObject setBar = commands.at(1).toObject();
    QJsonObject effect = setBar.value(QStringLiteral("SetContextBar")).toObject();
    QJsonObject bar = effect.value(QStringLiteral("bar")).toObject();
    bar.insert(QStringLiteral("info"), action("info", "Info"));
    effect.insert(QStringLiteral("bar"), bar);
    setBar.insert(QStringLiteral("SetContextBar"), effect);
    commands[1] = setBar;
    commands.append(QJsonObject{
        {"SetNavigation",
         QJsonObject{
             {"surface_id", "settings"},
             {"revision", 1},
             {"navigation",
              QJsonObject{{"items",
                           QJsonArray{QJsonObject{
                               {"interaction_id", "nav.settings"},
                               {"label", "Settings"},
                               {"accessibility_label", "Settings"},
                               {"icon_token", "gearshape"},
                               {"selected", true},
                               {"badge_count", 0}}}}}},
         }}});
    controller.dispatchCommands(commands);
    QApplication::processEvents();

    assert(controller.findChild<QPushButton *>("context-navigation") == nullptr);
    auto *info = controller.findChild<QPushButton *>("context-info");
    assert(info != nullptr);
    assert(info->text() == QStringLiteral("Info"));
    assert(controller.findChild<QPushButton *>("context-primary") != nullptr);

    auto *titleRow = controller.findChild<QWidget *>("surface-title-row");
    assert(titleRow != nullptr);
    assert(info->parentWidget() == titleRow);
}

// An information overlay is read, not chosen from: a dialog with Core's
// text and a close button, nothing else to activate.
void assertInformationOverlayShowsItsText() {
    PresentationController controller(nullptr);
    controller.resize(700, 600);
    controller.show();
    controller.dispatchCommands(baseCommands());
    controller.dispatchCommands(QJsonArray{QJsonObject{
        {"PresentOverlay",
         QJsonObject{
             {"surface_id", "settings"},
             {"revision", 1},
             {"overlay",
              QJsonObject{{"kind", "information"},
                          {"title", "Settings"},
                          {"items", QJsonArray{}},
                          {"body", "Here you change your name."},
                          {"close_label", "Schließen"}}},
         }}}});
    QApplication::processEvents();

    auto *dialog = controller.findChild<QDialog *>();
    assert(dialog != nullptr);
    assert(dialog->windowTitle() == QStringLiteral("Settings"));
    auto *body = dialog->findChild<QLabel *>("overlay-body");
    assert(body != nullptr);
    assert(body->text() == QStringLiteral("Here you change your name."));
    const auto buttons = dialog->findChildren<QPushButton *>();
    assert(buttons.size() == 1);
    // Core names the way out in the person's language (vauchi/private#479).
    assert(buttons.first()->text() == QStringLiteral("Schließen"));
    dialog->close();
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

void assertQrScanRequestOpensNonBlockingPastePrompt() {
    PresentationController controller(nullptr);
    controller.resize(700, 600);
    controller.show();
    // The test target never defines VAUCHI_HAS_CAMERA, so QrRequestScan
    // deterministically takes the no-camera paste fallback. A blocking
    // (exec-style) prompt would never return here, so reaching the
    // assertions at all is the non-blocking guarantee.
    controller.dispatchCommands(QJsonArray{QStringLiteral("QrRequestScan")});
    QApplication::processEvents();
    auto *dialog = controller.findChild<QInputDialog *>();
    assert(dialog != nullptr);
    assert(dialog->windowModality() == Qt::WindowModal);
    dialog->reject();
    QApplication::processEvents();
}

// Core computes the wait and sends it as `delay_millis`; the shell arms
// exactly that, whatever the other fields would suggest
// (vauchi/private#548).
void assertScheduleWakeupArmsCoresDelay() {
    PresentationController controller(nullptr);
    QVector<uint32_t> scheduled;
    QObject::connect(&controller, &PresentationController::wakeupScheduled,
                     &controller, [&scheduled](uint32_t milliseconds) {
                         scheduled.append(milliseconds);
                     });
    controller.dispatchCommands(QJsonArray{QJsonObject{
        {"ScheduleWakeup",
         QJsonObject{{"earliest_secs", 5},
                     {"deadline_secs", 10},
                     {"min_interval_secs", 1},
                     {"earliest_millis", QJsonValue::Null},
                     {"delay_millis", 2000}}}}});
    QApplication::processEvents();
    assert(scheduled.size() == 1);
    assert(scheduled.constFirst() == 2000);
}

QJsonObject qrScannedEvent(const QString &data) {
    return {{QStringLiteral("QrScanned"),
             QJsonObject{{QStringLiteral("data"), data}}}};
}

QJsonObject qrUnavailableEvent() {
    return {{QStringLiteral("HardwareUnavailable"),
             QJsonObject{{QStringLiteral("transport"),
                          QStringLiteral("qr_scan")}}}};
}

void assertQrPastePromptAcceptEmitsQrScanned() {
    QWidget parent;
    QrPastePrompt prompt(&parent);
    QVector<QJsonObject> events;
    QObject::connect(&prompt, &QrPastePrompt::eventReady, &parent,
                     [&events](const QJsonObject &event) {
                         events.append(event);
                     });
    prompt.prompt();
    QApplication::processEvents();
    auto *dialog = parent.findChild<QInputDialog *>();
    assert(dialog != nullptr);
    assert(dialog->windowModality() == Qt::WindowModal);
    dialog->setTextValue(QStringLiteral("  vauchi://qr-payload  "));
    dialog->accept();
    QApplication::processEvents();
    assert(events.size() == 1);
    assert(events.constFirst()
           == qrScannedEvent(QStringLiteral("vauchi://qr-payload")));
    // The prompt destroys the dialog with deleteLater(), which is the only
    // safe way to free it from inside its own finished handler. Plain
    // processEvents() does not drain DeferredDelete, so pump it explicitly
    // before asserting the dialog is gone.
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    assert(parent.findChild<QInputDialog *>() == nullptr);
}

void assertQrPastePromptCancelNotifiesCore() {
    QWidget parent;
    QrPastePrompt prompt(&parent);
    QVector<QJsonObject> events;
    QObject::connect(&prompt, &QrPastePrompt::eventReady, &parent,
                     [&events](const QJsonObject &event) {
                         events.append(event);
                     });
    prompt.prompt();
    QApplication::processEvents();
    auto *dialog = parent.findChild<QInputDialog *>();
    assert(dialog != nullptr);
    dialog->reject();
    QApplication::processEvents();
    assert(events.size() == 1);
    assert(events.constFirst() == qrUnavailableEvent());
}

void assertQrPastePromptEmptyAcceptNotifiesCore() {
    QWidget parent;
    QrPastePrompt prompt(&parent);
    QVector<QJsonObject> events;
    QObject::connect(&prompt, &QrPastePrompt::eventReady, &parent,
                     [&events](const QJsonObject &event) {
                         events.append(event);
                     });
    prompt.prompt();
    QApplication::processEvents();
    auto *dialog = parent.findChild<QInputDialog *>();
    assert(dialog != nullptr);
    dialog->accept();
    QApplication::processEvents();
    assert(events.size() == 1);
    assert(events.constFirst() == qrUnavailableEvent());
}

void assertQrPastePromptReusesOpenDialog() {
    QWidget parent;
    QrPastePrompt prompt(&parent);
    prompt.prompt();
    prompt.prompt();
    QApplication::processEvents();
    const auto dialogs = parent.findChildren<QInputDialog *>();
    assert(dialogs.size() == 1);
    dialogs.constFirst()->reject();
    QApplication::processEvents();
}

} // namespace

int main(int argc, char **argv) {
    QApplication app(argc, argv);
    assertExportPayloadUsesCanonicalSchema();
    assertNativeBarAndFocusRestoration();
    assertNoNavigationButtonBesideTheSidebarAndAnInfoButton();
    assertInformationOverlayShowsItsText();
    assertOverlayStructures(false);
    assertOverlayStructures(true);
    assertNavigationItemsRespectMinimumTargetSize();
    assertQrScanRequestOpensNonBlockingPastePrompt();
    assertQrPastePromptAcceptEmitsQrScanned();
    assertQrPastePromptCancelNotifiesCore();
    assertQrPastePromptEmptyAcceptNotifiesCore();
    assertQrPastePromptReusesOpenDialog();
    assertScheduleWakeupArmsCoresDelay();
    qunsetenv("QT_REDUCE_MOTION");
    return 0;
}
