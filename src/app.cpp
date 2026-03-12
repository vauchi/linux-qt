// SPDX-FileCopyrightText: 2026 Mattia Egloff <mattia.egloff@pm.me>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "app.h"
#include "coreui/screenrenderer.h"

#include <QHBoxLayout>
#include <QListWidget>
#include <QJsonDocument>
#include <QJsonArray>

VauchiWindow::VauchiWindow(QWidget *parent) : QMainWindow(parent) {
    setWindowTitle("Vauchi");
    resize(700, 600);

    m_app = vauchi_app_create();

    // Navigate to dynamic default screen (MyInfo with 0 contacts, Contacts with >=1)
    if (m_app) {
        char *defaultScreen = vauchi_app_default_screen(m_app);
        if (defaultScreen) {
            char *resultJson = vauchi_app_navigate_to(m_app, defaultScreen);
            if (resultJson) vauchi_string_free(resultJson);
            vauchi_string_free(defaultScreen);
        }
    }

    auto *central = new QWidget(this);
    auto *layout = new QHBoxLayout(central);

    // Sidebar
    m_sidebar = new QListWidget;
    m_sidebar->setFixedWidth(200);
    layout->addWidget(m_sidebar);

    // Content
    m_renderer = new ScreenRenderer(m_app, this);
    layout->addWidget(m_renderer, 1);

    setCentralWidget(central);

    buildSidebar();

    // Refresh sidebar when screen changes (e.g., after onboarding completes)
    connect(m_renderer, &ScreenRenderer::screenChanged, this, [this]() {
        refreshSidebar();
    });

    connect(m_sidebar, &QListWidget::currentRowChanged, this, [this](int row) {
        if (row < 0 || !m_app) return;

        // Get available screens
        char *screensJson = vauchi_app_available_screens(m_app);
        if (!screensJson) return;

        QJsonArray screens = QJsonDocument::fromJson(screensJson).array();
        vauchi_string_free(screensJson);

        if (row < screens.size()) {
            QString screenName = screens[row].toString();
            char *resultJson = vauchi_app_navigate_to(m_app, screenName.toUtf8().constData());
            if (resultJson) {
                vauchi_string_free(resultJson);
            }
            m_renderer->refresh();
        }
    });
}

VauchiWindow::~VauchiWindow() {
    if (m_app) {
        vauchi_app_destroy(m_app);
    }
}

void VauchiWindow::buildSidebar() {
    refreshSidebar();
}

void VauchiWindow::refreshSidebar() {
    m_sidebar->clear();
    if (!m_app) return;

    char *json = vauchi_app_available_screens(m_app);
    if (!json) return;

    QJsonArray screens = QJsonDocument::fromJson(json).array();
    vauchi_string_free(json);

    for (const auto &screen : screens) {
        QString name = screen.toString();
        // Capitalize first letter for display
        if (!name.isEmpty()) {
            name[0] = name[0].toUpper();
        }
        name.replace('_', ' ');
        m_sidebar->addItem(name);
    }
}
