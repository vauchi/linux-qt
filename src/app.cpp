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
#include <QJsonObject>
#include <QShortcut>
#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QMessageBox>
#include <QProcessEnvironment>
#include <QStatusBar>

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
    connect(menuBar, &VauchiMenuBar::importContactsRequested, this, [this]() {
        importContactsFromFile();
    });

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
                auto *window = static_cast<VauchiWindow *>(user_data);
                QMetaObject::invokeMethod(
                    window,
                    [window]() {
                        window->m_renderer->refresh();
                        window->drainAndShowNotifications();
                    },
                    Qt::QueuedConnection);
            },
            this);

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

        const QByteArray locale = systemLocaleCode().toUtf8();
        char *json = vauchi_app_sidebar_items(m_app, locale.constData());
        if (!json) return;

        QJsonArray tabs = QJsonDocument::fromJson(json).array();
        vauchi_string_free(json);

        if (row < tabs.size()) {
            QString screenId = tabs[row].toObject()["id"].toString();
            char *resultJson = vauchi_app_navigate_to(m_app, screenId.toUtf8().constData());
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

void VauchiWindow::refreshSidebar() {
    m_sidebar->clear();
    if (!m_app) return;

    // Core owns both the screen set and the localized labels — no
    // local SCREEN_I18N table needed. Each TabInfo.label is already
    // resolved against the requested locale, with an English
    // fallback baked in when the key is missing (see
    // `AppEngine::sidebar_items` in vauchi-app).
    const QByteArray locale = systemLocaleCode().toUtf8();
    char *json = vauchi_app_sidebar_items(m_app, locale.constData());
    if (!json) return;

    QJsonArray tabs = QJsonDocument::fromJson(json).array();
    vauchi_string_free(json);

    for (const auto &tab : tabs) {
        m_sidebar->addItem(tab.toObject()["label"].toString());
    }
}

void VauchiWindow::drainAndShowNotifications() {
    if (!m_app || !m_tray) return;

    char *json = vauchi_app_drain_notifications(m_app);
    if (!json) return;

    QJsonArray notifications = QJsonDocument::fromJson(json).array();
    vauchi_string_free(json);

    for (const auto &n : notifications) {
        QJsonObject obj = n.toObject();
        QString title = obj["title"].toString();
        QString body = obj["body"].toString();
        QSystemTrayIcon::MessageIcon icon =
            obj["category"].toString() == "EmergencyAlert"
                ? QSystemTrayIcon::Critical
                : QSystemTrayIcon::Information;
        m_tray->showMessage(title, body, icon, 10000);
    }
}

void VauchiWindow::importContactsFromFile() {
    if (!m_app) return;

    QString path = QFileDialog::getOpenFileName(
        this,
        tr_vauchi("contacts.importContacts",
                  QStringLiteral("Import Contacts")),
        QString(),
        QStringLiteral("vCard Files (*.vcf)"));
    if (path.isEmpty()) return;

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        statusBar()->showMessage(
            tr_vauchi("platform.error_could_not_read_file",
                      QStringLiteral("Could not read file")),
            4000);
        return;
    }

    QByteArray data = file.readAll();
    file.close();

    char *result = vauchi_app_import_contacts_from_vcf(
        m_app,
        reinterpret_cast<const uint8_t *>(data.constData()),
        static_cast<uintptr_t>(data.size()));

    if (!result) {
        statusBar()->showMessage(
            tr_vauchi("platform.error_import_failed",
                      QStringLiteral("Import failed")),
            4000);
        return;
    }

    QJsonObject obj = QJsonDocument::fromJson(result).object();
    vauchi_string_free(result);

    if (obj.contains("error")) {
        statusBar()->showMessage(
            tr_vauchi("platform.error_import_failed",
                      QStringLiteral("Import failed: "))
            + obj["error"].toString(),
            4000);
        return;
    }

    int imported = obj["imported"].toInt();
    int skipped = obj["skipped"].toInt();
    QString importedLine = tr_vauchi("import_contacts.result_imported",
                                     QStringLiteral("{count} contact(s) imported"))
                               .replace(QStringLiteral("{count}"),
                                        QString::number(imported));
    QString msg = importedLine;
    if (skipped > 0) {
        QString skippedLine = tr_vauchi("import_contacts.result_skipped",
                                        QStringLiteral("{count} skipped (duplicates or invalid)"))
                                  .replace(QStringLiteral("{count}"),
                                           QString::number(skipped));
        msg = importedLine + QStringLiteral(" — ") + skippedLine;
    }

    QJsonArray warnings = obj["warnings"].toArray();
    if (warnings.isEmpty()) {
        statusBar()->showMessage(msg, 4000);
    } else {
        QStringList lines;
        for (const auto &w : warnings) {
            QJsonObject wo = w.toObject();
            QString key = wo["key"].toString();
            QString legacyText = wo["legacy_text"].toString();
            QString rendered = tr_vauchi(key.toUtf8().constData(), legacyText);
            const QJsonObject args = wo["args"].toObject();
            for (auto it = args.begin(); it != args.end(); ++it) {
                rendered.replace(QStringLiteral("{%1}").arg(it.key()),
                                 it.value().toString());
            }
            lines << QStringLiteral("• ") + rendered;
        }
        QMessageBox::information(this, msg, lines.join(QStringLiteral("\n")));
    }

    // Refresh screen to show imported contacts
    m_renderer->refresh();
}

