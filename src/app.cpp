// SPDX-FileCopyrightText: 2026 Mattia Egloff <mattia.egloff@pm.me>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "app.h"
#include "coreui/screenrenderer.h"

VauchiApp::VauchiApp(QWidget *parent) : QMainWindow(parent) {
    setWindowTitle("Vauchi");
    resize(400, 700);

    m_workflow = vauchi_workflow_create("onboarding");
    m_renderer = new ScreenRenderer(m_workflow, this);
    setCentralWidget(m_renderer);
}

VauchiApp::~VauchiApp() {
    if (m_workflow) {
        vauchi_workflow_destroy(m_workflow);
    }
}
