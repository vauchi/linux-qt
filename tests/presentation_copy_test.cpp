// SPDX-FileCopyrightText: 2026 Mattia Egloff <mattia.egloff@pm.me>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "coreui/presentationcontroller.h"
#include "coreui/presentationeffectpayload.h"
#include "coreui/presentationsurface.h"

#include <QApplication>
#include <QAccessible>
#include <QDialog>
#include <QJsonArray>
#include <QInputDialog>
#include <QKeyEvent>
#include <QLineEdit>
#include <QPushButton>
#include <cassert>

namespace {

QJsonObject surface(const QJsonArray &nodes = {}) {
    return {{"surface_id", "prepared"}, {"revision", 1},
            {"title", "Surface"}, {"tokens", QJsonObject{}},
            {"nodes", nodes}};
}

QJsonObject replacement(const QJsonObject &value) {
    return {{"ReplaceSurface", QJsonObject{{"surface", value}}}};
}

// @internal
void assertPreparedWindowTitle() {
    PresentationController controller(nullptr);
    QJsonObject profile{{"primary_surface", "prepared"},
                        {"active_surface", "prepared"},
                        {"pane_layout", "single"},
                        {"window_title", "Titre fourni"}};
    controller.dispatchCommands({replacement(surface()),
        QJsonObject{{"SetPresentationProfile", QJsonObject{{"profile", profile}}}}});
    assert(controller.windowTitle() == QStringLiteral("Titre fourni"));

    profile.insert(QStringLiteral("window_title"), QStringLiteral("Autre titre"));
    controller.dispatchCommands({QJsonObject{{"SetPresentationProfile",
        QJsonObject{{"profile", profile}}}}});
    assert(controller.windowTitle() == QStringLiteral("Autre titre"));

    profile.remove(QStringLiteral("window_title"));
    controller.dispatchCommands({QJsonObject{{"SetPresentationProfile",
        QJsonObject{{"profile", profile}}}}});
    assert(controller.windowTitle() == QStringLiteral("Autre titre"));
    profile.insert(QStringLiteral("window_title"), QJsonValue::Null);
    controller.dispatchCommands({QJsonObject{{"SetPresentationProfile",
        QJsonObject{{"profile", profile}}}}});
    assert(controller.windowTitle() == QStringLiteral("Autre titre"));
    profile.insert(QStringLiteral("window_title"), QString());
    controller.dispatchCommands({QJsonObject{{"SetPresentationProfile",
        QJsonObject{{"profile", profile}}}}});
    assert(controller.windowTitle().isEmpty());
}

// @internal
void assertPreparedCaptureCopyAndSubmission() {
    for (bool supplied : {true, false}) {
        QJsonObject payload{{"id", "capture"}, {"purpose", "capture"},
                            {"label", "Instruction QR"},
                            {"accessibility", QJsonObject{{"label", "Lire le code"}}}};
        if (supplied) {
            payload.insert(QStringLiteral("capture_placeholder"), QStringLiteral("Collez ici"));
            payload.insert(QStringLiteral("capture_submit_label"), QStringLiteral("Valider"));
        }
        PresentationSurface widget(surface({QJsonObject{{"Qr", payload}}}));
        auto *input = widget.findChild<QLineEdit *>(QStringLiteral("capture"));
        assert(input != nullptr);
        assert(input->placeholderText() == (supplied ? QStringLiteral("Collez ici") : QString()));
        const auto buttons = widget.findChildren<QPushButton *>();
        assert(buttons.size() == 1);
        auto *button = buttons.first();
        assert(button->text() == (supplied ? QStringLiteral("Valider") : QStringLiteral("Lire le code")));
        assert(button->accessibleName() == QStringLiteral("Lire le code"));

        QJsonArray observations;
        QObject::connect(&widget, &PresentationSurface::valueReady, &widget,
            [&observations](const QString &surfaceId, const QString &bindingId,
                            const QJsonValue &value) {
                observations.append(QJsonObject{{"surface_id", surfaceId},
                    {"binding_id", bindingId}, {"value", value}});
            });
        input->setText(QStringLiteral("opaque QR data"));
        assert(observations.isEmpty());
        button->click();
        assert((observations == QJsonArray{QJsonObject{
            {"surface_id", "prepared"}, {"binding_id", "capture"},
            {"value", QJsonObject{{"text", "opaque QR data"}}}}}));
        QKeyEvent enter(QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier);
        QApplication::sendEvent(input, &enter);
        assert(observations.size() == 2);
        assert(observations.last() == observations.first());
    }
}

// @internal
void assertOptionalOverlayCloseCopy() {
    for (int variant = 0; variant < 3; ++variant) {
        PresentationController controller(nullptr);
        controller.dispatchCommands({replacement(surface())});
        QJsonObject info{{"kind", "information"}, {"title", "Information"},
                         {"body", "Texte fourni"}, {"items", QJsonArray{}}};
        if (variant == 1) info.insert(QStringLiteral("close_label"), QJsonValue::Null);
        if (variant == 2) info.insert(QStringLiteral("close_label"), QString());
        controller.dispatchCommands({QJsonObject{{"PresentOverlay", QJsonObject{
            {"surface_id", "prepared"}, {"revision", 1}, {"overlay", info}}}}});
        auto *dialog = controller.findChild<QDialog *>();
        assert(dialog != nullptr);
        const auto buttons = dialog->findChildren<QPushButton *>();
        assert(buttons.size() == 1);
        assert(buttons.first()->text().isEmpty());
        assert(!buttons.first()->icon().isNull());
        auto *accessible = QAccessible::queryAccessibleInterface(buttons.first());
        assert(accessible != nullptr);
        assert(accessible->text(QAccessible::Name) == QStringLiteral("Information"));
        buttons.first()->click();
        assert(!dialog->isVisible());
        QApplication::processEvents();
    }
}

// @internal
void assertScanOutcomeHasNoShellOwnedPrompt() {
    const QJsonObject unavailable{{"HardwareUnavailable",
        QJsonObject{{"transport", "qr_scan"}}}};
    assert(PresentationEffectPayload::qrScanResult(QString()) == unavailable);
    for (const QString &data : {QStringLiteral("opaque QR data"), QStringLiteral("  raw data  ")}) {
        const QJsonObject scanned{{"QrScanned", QJsonObject{{"data", data}}}};
        assert(PresentationEffectPayload::qrScanResult(data) == scanned);
    }
    PresentationController controller(nullptr);
    controller.dispatchCommands({replacement(surface()), QStringLiteral("QrRequestScan")});
    QApplication::processEvents();
    assert(controller.findChildren<QInputDialog *>().isEmpty());
    assert(controller.findChild<PresentationSurface *>() != nullptr);
}

} // namespace

int main(int argc, char **argv) {
    QApplication app(argc, argv);
    if (argc != 2) return 2;
    const QString test = QString::fromLocal8Bit(argv[1]);
    if (test == QStringLiteral("profile")) assertPreparedWindowTitle();
    else if (test == QStringLiteral("capture")) assertPreparedCaptureCopyAndSubmission();
    else if (test == QStringLiteral("overlay")) assertOptionalOverlayCloseCopy();
    else if (test == QStringLiteral("scan")) assertScanOutcomeHasNoShellOwnedPrompt();
    else return 2;
    return 0;
}
