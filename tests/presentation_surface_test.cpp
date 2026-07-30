// SPDX-FileCopyrightText: 2026 Mattia Egloff <mattia.egloff@pm.me>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "coreui/presentationsurface.h"

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QFrame>
#include <QJsonArray>
#include <QLabel>
#include <QLineEdit>
#include <QMetaObject>
#include <QProgressBar>
#include <QPushButton>
#include <QSlider>
#include <QToolButton>
#include <cassert>

static QJsonObject accessibility(const QString &label) {
    return {{"label", label}, {"description", QJsonValue::Null}};
}

static QJsonObject action(const QString &id, const QString &label) {
    return {
        {"interaction_id", id},
        {"label", label},
        {"accessibility_label", label},
        {"icon_token", QJsonValue::Null},
        {"enabled", true},
        {"shortcut", QJsonValue::Null},
    };
}

int main(int argc, char **argv) {
    QApplication app(argc, argv);
    const QJsonObject surface{
        {"surface_id", "settings"},
        {"revision", 2},
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
                              {"value", ""},
                              {"placeholder", QJsonValue::Null},
                              {"input_kind", "text"},
                              {"max_length", QJsonValue::Null},
                              {"validation_error", QJsonValue::Null},
                              {"enabled", true},
                              {"accessibility", accessibility("Name")}}}},
             QJsonObject{
                 {"Toggle",
                  QJsonObject{{"binding_id", "sharing"},
                              {"label", "Sharing"},
                              {"value", false},
                              {"enabled", true},
                              {"accessibility", accessibility("Sharing")}}}},
             QJsonObject{
                 {"Choice",
                  QJsonObject{
                      {"binding_id", "color"},
                      {"label", "Color"},
                      {"selected", "blue"},
                      {"options",
                       QJsonArray{
                           QJsonObject{{"id", "blue"}, {"label", "Blue"}},
                           QJsonObject{{"id", "green"}, {"label", "Green"}},
                       }},
                      {"enabled", true},
                      {"accessibility", accessibility("Color")},
                  }}},
             QJsonObject{
                 {"List",
                  QJsonObject{
                      {"id", "contacts"},
                      {"label", "Contacts"},
                      {"rows",
                       QJsonArray{QJsonObject{
                           {"title", "Alice"},
                           {"subtitle", QJsonValue::Null},
                           {"detail", QJsonValue::Null},
                           {"icon_token", QJsonValue::Null},
                           {"image_data", QJsonValue::Null},
                           {"fallback_text", QJsonValue::Null},
                           {"selected", false},
                           {"enabled", true},
                           {"activation", action("select-alice", "Alice")},
                           {"secondary_actions", QJsonArray{}},
                           {"controls", QJsonArray{}},
                           {"accessibility", accessibility("Alice")},
                       }}},
                      {"searchable", false},
                      {"paging", QJsonValue::Null},
                      {"accessibility", accessibility("Contacts")},
                  }}},
             QJsonObject{
                 {"Status",
                  QJsonObject{
                      {"id", QJsonValue::Null},
                      {"title", "Open"},
                      {"detail", QJsonValue::Null},
                      {"icon_token", QJsonValue::Null},
                      {"badge", QJsonValue::Null},
                      {"tone", "neutral"},
                      {"activation",
                       QJsonObject{{"interaction_id", "open"},
                                   {"label", "Open"},
                                   {"accessibility_label", "Open"},
                                   {"enabled", true},
                                   {"shortcut", QJsonValue::Null}}},
                      {"accessibility", accessibility("Open")}}}},
             QJsonObject{
                 {"Confirmation",
                  QJsonObject{
                      {"id", "remove"},
                      {"warning", "Remove this item?"},
                      {"confirm", action("confirm-remove", "Remove")},
                      {"cancel", action("cancel-remove", "Cancel")},
                      {"accessibility", accessibility("Remove item")},
                  }}},
             QJsonObject{
                 {"Image",
                  QJsonObject{
                      {"id", QJsonValue::Null},
                      {"data", QJsonValue::Null},
                      {"fallback_text", "No image"},
                      {"shape", "natural"},
                      {"brightness", 1.0},
                      {"activation", QJsonValue::Null},
                      {"accessibility", accessibility("Profile image")},
                  }}},
             QJsonObject{
                 {"Slider",
                  QJsonObject{
                      {"binding_id", "volume"},
                      {"label", "Volume"},
                      {"value", 0.5},
                      {"minimum", 0.0},
                      {"maximum", 1.0},
                      {"step", 0.1},
                      {"minimum_icon", QJsonValue::Null},
                      {"maximum_icon", QJsonValue::Null},
                      {"accessibility", accessibility("Volume")},
                  }}},
             QJsonObject{
                 {"Progress",
                  QJsonObject{
                      {"label", "Saving"},
                      {"value", 0.25},
                      {"accessibility", accessibility("Saving")},
                  }}},
             QJsonObject{
                 {"Qr",
                  QJsonObject{{"id", "display-code"},
                              {"payloads", QJsonArray{"vauchi://exchange"}},
                              {"purpose", "display"},
                              {"label", "Share code"},
                              {"accessibility",
                               accessibility("Share code")}}}},
             QJsonObject{
                 {"Qr",
                  QJsonObject{{"id", "scan-code"},
                              {"payloads", QJsonArray{}},
                              {"purpose", "capture"},
                              {"label", "Scan code"},
                              {"accessibility",
                               accessibility("Scan code")}}}},
             QJsonValue(QStringLiteral("Divider")),
         }},
    };
    PresentationSurface renderer(surface);
    QString interactionSurface;
    QString interaction;
    QString binding;
    QJsonValue value;
    QObject::connect(
        &renderer, &PresentationSurface::interactionReady,
        [&](const QString &surfaceId, const QString &interactionId) {
            interactionSurface = surfaceId;
            interaction = interactionId;
        });
    QObject::connect(
        &renderer, &PresentationSurface::valueReady,
        [&](const QString &, const QString &bindingId,
            const QJsonValue &input) {
            binding = bindingId;
            value = input;
        });

    auto *nameInput = renderer.findChild<QLineEdit *>("name");
    assert(nameInput != nullptr);
    nameInput->setText("Alice");
    assert(binding == QStringLiteral("name"));
    assert(value.toObject().value("text").toString()
           == QStringLiteral("Alice"));
    renderer.findChild<QCheckBox *>()->setChecked(true);
    assert(binding == QStringLiteral("sharing"));
    assert(value.toObject().value("boolean").toBool());

    auto *choice = renderer.findChild<QComboBox *>("color");
    assert(choice != nullptr);
    choice->setCurrentIndex(choice->findData(QStringLiteral("green")));
    assert(binding == QStringLiteral("color"));
    assert(value.toObject().value("choice").toString()
           == QStringLiteral("green"));

    renderer.findChild<QPushButton *>("open")->click();
    assert(interactionSurface == QStringLiteral("settings"));
    assert(interaction == QStringLiteral("open"));
    renderer.findChild<QToolButton *>("select-alice")->click();
    assert(interaction == QStringLiteral("select-alice"));
    renderer.findChild<QPushButton *>("confirm-remove")->click();
    assert(interaction == QStringLiteral("confirm-remove"));

    auto *slider = renderer.findChild<QSlider *>("volume");
    assert(slider != nullptr);
    slider->setValue(slider->maximum());
    assert(binding == QStringLiteral("volume"));
    assert(value.toObject().value("number").toDouble() == 1.0);
    auto *progress = renderer.findChild<QProgressBar *>();
    assert(progress != nullptr);
    assert(progress->value() == 250);
    assert(progress->accessibleName() == QStringLiteral("Saving"));
    assert(!renderer.findChildren<QFrame *>().isEmpty());

    QLabel *qrImage = nullptr;
    for (auto *label : renderer.findChildren<QLabel *>()) {
        if (label->accessibleName() == QStringLiteral("Share code")) {
            qrImage = label;
            break;
        }
    }
    assert(qrImage != nullptr);
    assert(!qrImage->pixmap(Qt::ReturnByValue).isNull());

    auto *qrCapture = renderer.findChild<QLineEdit *>("scan-code");
    assert(qrCapture != nullptr);
    qrCapture->setText(QStringLiteral("vauchi://scanned"));
    QMetaObject::invokeMethod(qrCapture, "editingFinished",
                              Qt::DirectConnection);
    assert(binding == QStringLiteral("scan-code"));
    assert(value.toObject().value("text").toString()
           == QStringLiteral("vauchi://scanned"));
    return 0;
}
