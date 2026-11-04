// SPDX-FileCopyrightText: 2026 Mattia Egloff <mattia.egloff@pm.me>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "presentationstate.h"
#include "vauchi.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QPointer>
#include <QWidget>

class HardwareBackend;
class FilePicker;
class PresentationSurface;

/// Applies Core reducer batches atomically and renders native Qt equivalents.
class PresentationController : public QWidget {
    Q_OBJECT

public:
    explicit PresentationController(struct ::VauchiApp *app,
                                    QWidget *parent = nullptr);
    void initialize();
    void refresh();
    void appBackgrounded();
    void deepLinkOpened(const QString &uri);
    void reportEnvironment(int width, int height);
    void reportNetworkCost(const QString &cost);
    void dispatchCommands(const QJsonArray &commands);
    void requestBack();
    void reportApplicationMenuAvailability(bool available);
    void activateAction(const QString &surfaceId, const QString &interactionId);

signals:
    void presentationChanged();
    void nativeBackRequested();
    void applicationMenuChanged(const QJsonObject &payload);
    /// Core's requested delay before the next wakeup, in milliseconds
    /// (vauchi/private#450: sub-second during a live QR exchange).
    void wakeupScheduled(uint32_t milliseconds);

private:
    void dispatchEvent(const QJsonObject &event);
    void dispatchRawEvent(const QJsonValue &event);
    void dispatchEncodedEvent(const QByteArray &eventJson);
    void dispatchInteraction(const QString &surfaceId,
                             const QString &interactionId);
    void dispatchInputSubmitted(const QString &surfaceId,
                                const QString &bindingId);
    void dispatchInputFocusEnded(const QString &surfaceId,
                                 const QString &bindingId);
    void dispatchBindingGesture(const QString &variant,
                                const QString &surfaceId,
                                const QString &bindingId);
    void dispatchValue(const QString &surfaceId, const QString &bindingId,
                       const QJsonValue &value);
    void applyEnvelope(const QByteArray &json);
    void applyBatch(const QJsonArray &commands);
    void renderPresentation();
    void presentOverlay(const QString &surfaceId,
                        const QJsonObject &overlay);
    void executeEffect(const QJsonValue &command);
    void pickFile(const QJsonObject &payload);
    void pickImage();
    static QJsonArray bytesToJson(const QByteArray &bytes);

    struct ::VauchiApp *m_app;
    PresentationState m_state;
    // The single surface shown and the navigation it was rendered with: a
    // same-revision replacement updates it in place (vauchi/private#1062).
    QPointer<PresentationSurface> m_singleSurface;
    QJsonObject m_renderedNavigation;
    HardwareBackend *m_hardware;
    FilePicker *m_filePicker;
    bool m_reducedMotion = false;
};
