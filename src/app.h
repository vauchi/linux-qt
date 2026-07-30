// SPDX-FileCopyrightText: 2026 Mattia Egloff <mattia.egloff@pm.me>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QJsonObject>
#include <QMainWindow>
#include "vauchi.h"

class QKeyEvent;
class QResizeEvent;
class QTimer;
class PresentationController;

class VauchiWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit VauchiWindow(QWidget *parent = nullptr);
    ~VauchiWindow() override;

protected:
    void changeEvent(QEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    void drainAndShowNotifications();
    void drainAndShowNotificationsArray(const QJsonArray &notifications);
    void importContactsFromFile();
    void scheduleWakeup(uint32_t seconds);
    void onWakeup();

    struct ::VauchiApp *m_app = nullptr;
    PresentationController *m_presentation = nullptr;
    class SystemTray *m_tray = nullptr;
    QTimer *m_wakeupTimer = nullptr;
};
