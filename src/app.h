// SPDX-FileCopyrightText: 2026 Mattia Egloff <mattia.egloff@pm.me>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QMainWindow>
#include "vauchi.h"

class ScreenRenderer;
class QListWidget;

class VauchiWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit VauchiWindow(QWidget *parent = nullptr);
    ~VauchiWindow() override;

private:
    void buildSidebar();
    void refreshSidebar();

    struct ::VauchiApp *m_app = nullptr;
    ScreenRenderer *m_renderer = nullptr;
    QListWidget *m_sidebar = nullptr;
};
