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
    /// Return pressed in a field — Qt's own submit gesture.
    void submitReady(const QString &surfaceId, const QString &bindingId);
    /// A field lost focus. Qt reports this whatever took the focus, so no
    /// click-outside handling is needed.
    void focusEndReady(const QString &surfaceId, const QString &bindingId);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    QWidget *renderNode(const QJsonValue &node);
    QWidget *renderList(const QJsonObject &payload);
    QWidget *renderGroup(const QJsonObject &payload);
    QWidget *renderChoice(const QJsonObject &payload);
    QWidget *renderStatus(const QJsonObject &payload);
    QWidget *renderConfirmation(const QJsonObject &payload);
    QWidget *renderImage(const QJsonObject &payload);
    QWidget *renderQr(const QJsonObject &payload);
    QWidget *actionButton(const QJsonObject &action);
    void activate(const QJsonObject &action);
    QString targetSizeStyleSheet() const;
    static void applyAccessibility(QWidget *widget,
                                   const QJsonObject &accessibility);

    QString m_surfaceId;
    // PresentationTokens defaults (prepared_surface.rs): touch_target.minimum
    // and border_radius.md_lg. Real surfaces always carry `tokens`; these
    // only cover a fixture that omits the field.
    int m_minimumTargetSize = 48;
    int m_cornerRadius = 12;
};
