// SPDX-FileCopyrightText: 2026 Mattia Egloff <mattia.egloff@pm.me>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QJsonObject>
#include <QMainWindow>
#include "vauchi.h"

class QKeyEvent;
class QTimer;
class ScreenRenderer;
class QListWidget;

class VauchiWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit VauchiWindow(QWidget *parent = nullptr);
    ~VauchiWindow() override;

protected:
    void changeEvent(QEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;

private:
    void buildSidebar();
    void refreshSidebar();
    void updateSidebarForScreen(const QJsonObject &screen);
    void drainAndShowNotifications();
    void drainAndShowNotificationsArray(const QJsonArray &notifications);
    void importContactsFromFile();
    void scheduleWakeup(uint32_t seconds);
    void onWakeup();

    struct ::VauchiApp *m_app = nullptr;
    ScreenRenderer *m_renderer = nullptr;
    QListWidget *m_sidebar = nullptr;
    class SystemTray *m_tray = nullptr;
    QTimer *m_wakeupTimer = nullptr;
};
