// SPDX-FileCopyrightText: 2026 Mattia Egloff <mattia.egloff@pm.me>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "app.h"
#include "coreui/screenrenderer.h"
#include "coreui/thememanager.h"
#include "platform/menubar.h"
#include "platform/systemtray.h"

#include <QApplication>
#include <QHBoxLayout>
#include <QListWidget>
#include <QJsonDocument>
#include <QJsonArray>
#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QProcessEnvironment>

VauchiWindow::VauchiWindow(QWidget *parent) : QMainWindow(parent) {
    setWindowTitle("Vauchi");
    resize(700, 600);

    // Apply core theme colors via QPalette (runtime-switchable)
    ThemeManager::applyDefaultTheme();

    // Persistent storage: XDG_DATA_HOME/vauchi/
    QString dataDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dataDir);

    // Relay URL: read from data_dir/relay_url.txt or VAUCHI_RELAY_URL env var
    QString relayUrl;
    QFile relayFile(dataDir + "/relay_url.txt");
    if (relayFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        relayUrl = QString::fromUtf8(relayFile.readAll()).trimmed();
        relayFile.close();
    }
    if (relayUrl.isEmpty()) {
        relayUrl = QProcessEnvironment::systemEnvironment().value("VAUCHI_RELAY_URL");
    }

    QByteArray dataDirUtf8 = dataDir.toUtf8();
    QByteArray relayUtf8 = relayUrl.toUtf8();
    const char *relayPtr = relayUrl.isEmpty() ? nullptr : relayUtf8.constData();

    // Try keyring-backed init first (uses D-Bus Secret Service for key storage),
    // fall back to config-only if keyring is unavailable or CABI was built without it.
    m_app = vauchi_app_create_with_keyring(dataDirUtf8.constData(), relayPtr);
    if (!m_app) {
        m_app = vauchi_app_create_with_config(dataDirUtf8.constData(), relayPtr);
    }

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
    m_sidebar->setObjectName(QStringLiteral("sidebar"));
    m_sidebar->setAccessibleName(QStringLiteral("Navigation"));
    layout->addWidget(m_sidebar);

    // Content
    m_renderer = new ScreenRenderer(m_app, this);
    layout->addWidget(m_renderer, 1);

    setCentralWidget(central);

    // Menu bar
    auto *menuBar = new VauchiMenuBar(this);
    setMenuBar(menuBar);
    connect(menuBar, &VauchiMenuBar::quitRequested, qApp, &QApplication::quit);

    // System tray
    auto *tray = new SystemTray(this);
    connect(tray, &SystemTray::showWindowRequested, this, &QWidget::show);
    connect(tray, &SystemTray::quitRequested, qApp, &QApplication::quit);
    tray->show();

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

// Sidebar labels matching core i18n nav.* keys (en.json).
// When CABI gains i18n exports (T2-5), replace with runtime calls.
static const QHash<QString, QString> SCREEN_LABELS = {
    {"my_info",        "My Card"},
    {"contacts",       "Contacts"},
    {"exchange",       "Exchange"},
    {"groups",         "Groups"},
    {"more",           "More"},
    {"onboarding",     "Onboarding"},
    {"settings",       "Settings"},
    {"help",           "Help"},
    {"recovery",       "Recovery"},
    {"backup",         "Backup"},
    {"sync",           "Sync"},
    {"privacy",        "Privacy"},
    {"device_linking", "Devices"},
    {"support",        "Support"},
};

static QString screenLabel(const QString &screenId) {
    auto it = SCREEN_LABELS.find(screenId);
    if (it != SCREEN_LABELS.end()) return it.value();
    // Fallback: capitalize + replace underscores
    QString label = screenId;
    if (!label.isEmpty()) label[0] = label[0].toUpper();
    label.replace('_', ' ');
    return label;
}

void VauchiWindow::refreshSidebar() {
    m_sidebar->clear();
    if (!m_app) return;

    char *json = vauchi_app_available_screens(m_app);
    if (!json) return;

    QJsonArray screens = QJsonDocument::fromJson(json).array();
    vauchi_string_free(json);

    for (const auto &screen : screens) {
        m_sidebar->addItem(screenLabel(screen.toString()));
    }
}
