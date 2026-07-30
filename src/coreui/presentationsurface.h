// SPDX-FileCopyrightText: 2026 Mattia Egloff <mattia.egloff@pm.me>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QJsonObject>
#include <QWidget>

class QBoxLayout;

/// Domain-free native renderer for one Core SurfaceSpec.
class PresentationSurface : public QWidget {
    Q_OBJECT

public:
    explicit PresentationSurface(const QJsonObject &surface,
                                 QWidget *parent = nullptr);

signals:
    void interactionReady(const QString &surfaceId,
                          const QString &interactionId);
    void valueReady(const QString &surfaceId, const QString &bindingId,
                    const QJsonValue &value);

private:
    QWidget *renderNode(const QJsonObject &node);
    QWidget *renderList(const QJsonObject &payload);
    QWidget *renderGroup(const QJsonObject &payload);
    QWidget *renderStatus(const QJsonObject &payload);
    QWidget *renderConfirmation(const QJsonObject &payload);
    QWidget *renderImage(const QJsonObject &payload);
    QWidget *renderQr(const QJsonObject &payload);
    QWidget *actionButton(const QJsonObject &action);
    void activate(const QJsonObject &action);
    static void applyAccessibility(QWidget *widget,
                                   const QJsonObject &accessibility);

    QString m_surfaceId;
};
