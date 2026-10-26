// SPDX-FileCopyrightText: 2026 Mattia Egloff <mattia.egloff@pm.me>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once
#include <QMenuBar>
#include <QJsonObject>

class QAction;
class QMenu;

class VauchiMenuBar : public QMenuBar {
    Q_OBJECT
public:
    explicit VauchiMenuBar(QWidget *parent = nullptr);
    void applyApplicationMenu(const QJsonObject &payload);

signals:
    void actionActivated(const QString &surfaceId, const QString &interactionId);
    void quitRequested();
    void aboutRequested();
private:
    QMenu *m_fileMenu;
    QMenu *m_helpMenu;
    QAction *m_fileSeparator;
    QList<QAction *> m_preparedActions;
    QList<QMenu *> m_preparedMenus;
};
