// SPDX-FileCopyrightText: 2026 Mattia Egloff <mattia.egloff@pm.me>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "presentationstate.h"
#include "vauchi.h"

#include <QJsonArray>
#include <QWidget>

class HardwareBackend;
class QBoxLayout;

/// Applies Core reducer batches atomically and renders native Qt equivalents.
class PresentationController : public QWidget {
    Q_OBJECT

public:
    explicit PresentationController(struct ::VauchiApp *app,
                                    QWidget *parent = nullptr);
    void initialize();
    void refresh();
    void appBackgrounded();
    void reportEnvironment(int width, int height);
    void dispatchCommands(const QJsonArray &commands);
    void requestBack();

signals:
    void presentationChanged();
    void nativeBackRequested();
    void wakeupScheduled(uint32_t seconds);

private:
    void dispatchEvent(const QJsonObject &event);
    void dispatchRawEvent(const QJsonValue &event);
    void dispatchInteraction(const QString &surfaceId,
                             const QString &interactionId);
    void dispatchValue(const QString &surfaceId, const QString &bindingId,
                       const QJsonValue &value);
    void applyEnvelope(const QByteArray &json);
    void applyBatch(const QJsonArray &commands);
    void renderPresentation();
    void renderContextBar(QBoxLayout *layout);
    void presentOverlay(const QString &surfaceId,
                        const QJsonObject &overlay);
    void executeEffect(const QJsonValue &command);
    void pickFile(const QJsonObject &payload);
    void pickImage();
    static QJsonArray bytesToJson(const QByteArray &bytes);

    struct ::VauchiApp *m_app;
    PresentationState m_state;
    HardwareBackend *m_hardware;
    bool m_reducedMotion = false;
};
