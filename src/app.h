// SPDX-FileCopyrightText: 2026 Mattia Egloff <mattia.egloff@pm.me>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QMainWindow>
#include "vauchi.h"

class ScreenRenderer;

class VauchiApp : public QMainWindow {
    Q_OBJECT

public:
    explicit VauchiApp(QWidget *parent = nullptr);
    ~VauchiApp() override;

private:
    VauchiWorkflow *m_workflow = nullptr;
    ScreenRenderer *m_renderer = nullptr;
};
