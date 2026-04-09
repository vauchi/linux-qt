// SPDX-FileCopyrightText: 2026 Mattia Egloff <mattia.egloff@pm.me>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "app.h"
#include "i18n.h"
#include "coreui/screenrenderer.h"
#include "coreui/thememanager.h"
#include "platform/menubar.h"
#include "platform/systemtray.h"

#include <QApplication>
#include <QHBoxLayout>
#include <QListWidget>
#include <QJsonDocument>
#include <QJsonArray>
#include <QShortcut>
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

    // Initialise i18n from bundled locale files.
    // Looks for locales/ next to the binary first (dev/local build),
    // then /usr/share/vauchi/locales (system install).
    // Uses dlsym-resolved wrappers — gracefully no-ops when the CABI
    // library lacks the i18n symbols.
    if (!vauchiI18nIsInitialized()) {
        QString localesDir = QCoreApplication::applicationDirPath()
                             + QStringLiteral("/../locales");
        if (!QDir(localesDir).exists()) {
            localesDir =
                QStringLiteral("/usr/share/vauchi/locales");
        }
        if (QDir(localesDir).exists()) {
            vauchiI18nInit(localesDir.toUtf8().constData());
        }
    }

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

#ifndef NDEBUG
    // --reset-for-testing: create a test identity so the app skips onboarding.
    if (m_app && QCoreApplication::arguments().contains(
            QStringLiteral("--reset-for-testing"))) {
        if (vauchi_app_has_identity(m_app) != 1) {
            int32_t rc = vauchi_app_create_identity(m_app, "Test User");
            if (rc != 0) {
                qWarning("[Vauchi] --reset-for-testing: failed to create identity");
            }
        }
    }
#endif

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
    m_tray = new SystemTray(this);
    connect(m_tray, &SystemTray::showWindowRequested, this, &QWidget::show);
    connect(m_tray, &SystemTray::quitRequested, qApp, &QApplication::quit);
    m_tray->show();

    // Register event callback for background screen invalidation (Plan 2D).
    // Core events (sync, contact updates, etc.) trigger re-render of the
    // active screen. The callback fires on the dispatching thread, so we
    // use QMetaObject::invokeMethod to marshal refresh() to the main thread.
    if (m_app) {
        vauchi_app_set_event_callback(
            m_app,
            [](const char * /*screen_ids_json*/, void *user_data) {
                auto *renderer = static_cast<ScreenRenderer *>(user_data);
                QMetaObject::invokeMethod(
                    renderer, &ScreenRenderer::refresh, Qt::QueuedConnection);
            },
            m_renderer);

    }

    buildSidebar();

    // Refresh sidebar when screen changes (e.g., after onboarding completes)
    connect(m_renderer, &ScreenRenderer::screenChanged, this, [this]() {
        refreshSidebar();
    });

    // Keyboard shortcuts: Alt+1..5 navigate sidebar screens
    for (int i = 0; i < 5; ++i) {
        auto *sc = new QShortcut(
            QKeySequence(Qt::ALT | static_cast<Qt::Key>(Qt::Key_1 + i)),
            this);
        connect(sc, &QShortcut::activated, this, [this, i]() {
            if (m_sidebar->count() > i) {
                m_sidebar->setCurrentRow(i);
            }
        });
    }

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

void VauchiWindow::changeEvent(QEvent *event) {
    QMainWindow::changeEvent(event);
    if (event->type() == QEvent::WindowDeactivate && m_app) {
        char *screenJson = vauchi_app_handle_app_backgrounded(m_app);
        if (screenJson) {
            vauchi_string_free(screenJson);
            m_renderer->refresh();
        }
    }
}

VauchiWindow::~VauchiWindow() {
    if (m_app) {
        vauchi_app_destroy(m_app);
    }
}

void VauchiWindow::buildSidebar() {
    refreshSidebar();
}

// Maps screen IDs to i18n keys (nav.* namespace in locales/*.json).
// Fallback strings are English defaults for when i18n is not initialised
// or the CABI library lacks the i18n symbols at link time.
static const QHash<QString, QPair<const char *, QString>> SCREEN_I18N = {
    {"my_info",        {"nav.myCard",    QStringLiteral("My Card")}},
    {"contacts",       {"nav.contacts",  QStringLiteral("Contacts")}},
    {"exchange",       {"nav.exchange",  QStringLiteral("Exchange")}},
    {"groups",         {"nav.groups",    QStringLiteral("Groups")}},
    {"more",           {"nav.more",      QStringLiteral("More")}},
    {"onboarding",     {"nav.onboarding", QStringLiteral("Onboarding")}},
    {"settings",       {"nav.settings",  QStringLiteral("Settings")}},
    {"help",           {"nav.help",      QStringLiteral("Help")}},
    {"recovery",       {"nav.recovery",  QStringLiteral("Recovery")}},
    {"backup",         {"nav.backup",    QStringLiteral("Backup")}},
    {"sync",           {"nav.sync",      QStringLiteral("Sync")}},
    {"activity_log",   {"nav.activity",  QStringLiteral("Activity")}},
    {"privacy",        {"nav.privacy",   QStringLiteral("Privacy")}},
    {"device_linking", {"nav.devices",   QStringLiteral("Devices")}},
    {"support",        {"nav.support",   QStringLiteral("Support")}},
};

static QString screenLabel(const QString &screenId) {
    auto it = SCREEN_I18N.find(screenId);
    if (it != SCREEN_I18N.end()) {
        const char *key = it.value().first;
        if (key) return tr_vauchi(key, it.value().second);
        return it.value().second;
    }
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

