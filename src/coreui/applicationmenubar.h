// SPDX-FileCopyrightText: 2026 Mattia Egloff <mattia.egloff@pm.me>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QJsonObject>
#include <QMenuBar>

class ApplicationMenuBar : public QMenuBar {
    Q_OBJECT
public:
    explicit ApplicationMenuBar(QWidget *parent = nullptr);
    void apply(const QJsonObject &payload);

signals:
    void actionActivated(const QString &surfaceId, const QString &interactionId);
    void quitRequested();

private:
    QAction *m_quit;
    QList<QMenu *> m_menus;
};
