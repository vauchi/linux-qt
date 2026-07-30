// SPDX-FileCopyrightText: 2026 Mattia Egloff <mattia.egloff@pm.me>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "presentationcontroller.h"

#include "presentationsurface.h"
#include "../platform/hardwarebackend.h"

#include <QApplication>
#include <QBoxLayout>
#include <QInputDialog>
#include <QJsonDocument>
#include <QKeySequence>
#include <QPushButton>
#include <QSplitter>

PresentationController::PresentationController(struct VauchiApp *app,
                                               QWidget *parent)
    : QWidget(parent), m_app(app),
      m_hardware(new HardwareBackend(this)),
      m_reducedMotion(qEnvironmentVariableIsSet("QT_REDUCE_MOTION")) {
    connect(m_hardware, &HardwareBackend::eventReady, this,
            [this](const QJsonObject &event) { dispatchEvent(event); });
    connect(m_hardware, &HardwareBackend::qrScanned, this,
            [this](const QString &data) {
                QString payload = data;
                if (payload.isEmpty()) {
                    bool accepted = false;
                    payload = QInputDialog::getMultiLineText(
                        this, tr("Scan QR"), tr("Paste QR data:"), {},
                        &accepted);
                    if (!accepted) {
                        return;
                    }
                }
                dispatchEvent(QJsonObject{
                    {QStringLiteral("QrScanned"),
                     QJsonObject{{QStringLiteral("data"), payload}}}});
            });
}

void PresentationController::initialize() {
    if (!m_app) {
        return;
    }
    char *json = vauchi_app_initial_commands(m_app);
    if (!json) {
        return;
    }
    const QByteArray envelope(json);
    vauchi_string_free(json);
    applyEnvelope(envelope);
    reportEnvironment(width(), height());
}

void PresentationController::refresh() {
    initialize();
}

void PresentationController::appBackgrounded() {
    dispatchRawEvent(QJsonValue(QStringLiteral("AppBackgrounded")));
}

void PresentationController::reportEnvironment(int width, int height) {
    dispatchEvent(QJsonObject{
        {QStringLiteral("PresentationEnvironmentChanged"),
         QJsonObject{
             {QStringLiteral("available_width"), qMax(0, width)},
             {QStringLiteral("available_height"), qMax(0, height)},
             {QStringLiteral("input_modes"),
              QJsonArray{QStringLiteral("pointer"),
                         QStringLiteral("keyboard")}},
             {QStringLiteral("motion"),
              m_reducedMotion ? QStringLiteral("reduced")
                              : QStringLiteral("full")},
         }}});
}

void PresentationController::dispatchCommands(const QJsonArray &commands) {
    applyBatch(commands);
}

void PresentationController::requestBack() {
    const QString surfaceId = m_state.activeSurfaceId();
    if (surfaceId.isEmpty()) {
        return;
    }
    dispatchEvent(QJsonObject{
        {QStringLiteral("SurfaceActivated"),
         QJsonObject{{QStringLiteral("surface_id"), surfaceId}}}});
    dispatchEvent(QJsonObject{
        {QStringLiteral("BackRequested"),
         QJsonObject{{QStringLiteral("surface_id"), surfaceId}}}});
}

void PresentationController::dispatchEvent(const QJsonObject &event) {
    dispatchRawEvent(event);
}

void PresentationController::dispatchInteraction(
    const QString &surfaceId, const QString &interactionId) {
    if (surfaceId.isEmpty() || interactionId.isEmpty()) {
        return;
    }
    dispatchEvent(QJsonObject{
        {QStringLiteral("SurfaceActivated"),
         QJsonObject{{QStringLiteral("surface_id"), surfaceId}}}});
    dispatchEvent(QJsonObject{
        {QStringLiteral("ActionActivated"),
         QJsonObject{{QStringLiteral("surface_id"), surfaceId},
                     {QStringLiteral("interaction_id"), interactionId}}}});
}

void PresentationController::dispatchValue(
    const QString &surfaceId, const QString &bindingId,
    const QJsonValue &value) {
    if (surfaceId.isEmpty() || bindingId.isEmpty()) {
        return;
    }
    dispatchEvent(QJsonObject{
        {QStringLiteral("SurfaceActivated"),
         QJsonObject{{QStringLiteral("surface_id"), surfaceId}}}});
    dispatchEvent(QJsonObject{
        {QStringLiteral("ValueChanged"),
         QJsonObject{{QStringLiteral("surface_id"), surfaceId},
                     {QStringLiteral("binding_id"), bindingId},
                     {QStringLiteral("value"), value}}}});
}

void PresentationController::applyEnvelope(const QByteArray &json) {
    const QJsonObject envelope = QJsonDocument::fromJson(json).object();
    if (envelope.contains(QStringLiteral("commands"))) {
        applyBatch(envelope.value(QStringLiteral("commands")).toArray());
    }
}

void PresentationController::applyBatch(const QJsonArray &commands) {
    QJsonArray effects;
    bool didChange = false;
    for (const auto &command : commands) {
        if (command.isObject()) {
            const QJsonObject object = command.toObject();
            const bool presentation =
                object.contains(QStringLiteral("ReplaceSurface"))
                || object.contains(QStringLiteral("SetContextBar"))
                || object.contains(QStringLiteral("SetPresentationProfile"));
            const bool overlay =
                object.contains(QStringLiteral("PresentOverlay"));
            if ((presentation || overlay) && m_state.apply(object)) {
                didChange = didChange || presentation;
                if (overlay) {
                    effects.append(command);
                }
                continue;
            }
        }
        effects.append(command);
    }
    if (didChange) {
        renderPresentation();
        emit presentationChanged();
    }
    for (const auto &effect : effects) {
        executeEffect(effect);
    }
}

void PresentationController::renderPresentation() {
    QString focusObjectName;
    if (QWidget *focused = QApplication::focusWidget();
        focused != nullptr
        && (focused == this || isAncestorOf(focused))) {
        focusObjectName = focused->objectName();
    }
    if (QLayout *oldLayout = layout()) {
        while (QLayoutItem *item = oldLayout->takeAt(0)) {
            delete item->widget();
            delete item;
        }
        delete oldLayout;
    }
    auto *outer = new QVBoxLayout(this);
    const QStringList visible = m_state.visibleSurfaceIds();
    QWidget *body = nullptr;
    if (visible.size() > 1) {
        auto *splitter = new QSplitter(Qt::Horizontal);
        for (const QString &surfaceId : visible) {
            if (const auto surface = m_state.surface(surfaceId)) {
                auto *widget = new PresentationSurface(*surface);
                connect(widget, &PresentationSurface::interactionReady, this,
                        &PresentationController::dispatchInteraction);
                connect(widget, &PresentationSurface::valueReady, this,
                        &PresentationController::dispatchValue);
                splitter->addWidget(widget);
            }
        }
        body = splitter;
    } else if (!visible.isEmpty()) {
        if (const auto surface = m_state.surface(visible.constFirst())) {
            auto *widget = new PresentationSurface(*surface);
            connect(widget, &PresentationSurface::interactionReady, this,
                    &PresentationController::dispatchInteraction);
            connect(widget, &PresentationSurface::valueReady, this,
                    &PresentationController::dispatchValue);
            body = widget;
        }
    }
    if (!body) {
        body = new QWidget;
    }
    outer->addWidget(body, 1);
    renderContextBar(outer);
    if (!focusObjectName.isEmpty()) {
        if (QWidget *restored =
                findChild<QWidget *>(focusObjectName);
            restored != nullptr) {
            restored->setFocus(Qt::OtherFocusReason);
        }
    }
}

void PresentationController::renderContextBar(QBoxLayout *layout) {
    const QJsonObject bar = m_state.contextBar();
    auto *strip = new QWidget;
    strip->setObjectName(QStringLiteral("contextual-command-bar"));
    auto *row = new QHBoxLayout(strip);
    const QString surfaceId = m_state.activeSurfaceId();
    for (const QString &role :
         {QStringLiteral("back"), QStringLiteral("navigation"),
          QStringLiteral("primary"), QStringLiteral("secondary")}) {
        const QJsonObject action = bar.value(role).toObject();
        if (action.isEmpty()) {
            continue;
        }
        auto *button =
            new QPushButton(action.value(QStringLiteral("label")).toString());
        button->setObjectName(QStringLiteral("context-") + role);
        button->setAccessibleName(
            action.value(QStringLiteral("accessibility_label")).toString());
        button->setEnabled(
            action.value(QStringLiteral("enabled")).toBool(true));
        button->setProperty(
            "tone", action.value(QStringLiteral("tone")).toString());
        if (role == QStringLiteral("primary")) {
            button->setSizePolicy(QSizePolicy::Expanding,
                                  QSizePolicy::Preferred);
            button->setDefault(true);
        }
        const QString shortcut =
            action.value(QStringLiteral("shortcut")).toString();
        if (shortcut == QStringLiteral("back")) {
            button->setShortcut(QKeySequence::Back);
        } else if (shortcut == QStringLiteral("undo")) {
            button->setShortcut(QKeySequence::Undo);
        } else if (shortcut == QStringLiteral("activate_primary")) {
            button->setShortcut(QKeySequence(QStringLiteral("Ctrl+Return")));
        } else if (role == QStringLiteral("navigation")) {
            button->setShortcut(QKeySequence(QStringLiteral("Ctrl+K")));
        } else if (role == QStringLiteral("secondary")) {
            button->setShortcut(QKeySequence(QStringLiteral("Alt+Down")));
        }
        const QString interaction =
            action.value(QStringLiteral("interaction_id")).toString();
        connect(button, &QPushButton::clicked, this,
                [this, surfaceId, interaction]() {
                    dispatchInteraction(surfaceId, interaction);
                });
        row->addWidget(button);
    }
    layout->addWidget(strip);
}
