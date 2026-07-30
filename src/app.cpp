// SPDX-FileCopyrightText: 2026 Mattia Egloff <mattia.egloff@pm.me>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "app.h"
#include "i18n.h"
#include "coreui/presentationcontroller.h"
#include "coreui/thememanager.h"
#include "platform/menubar.h"
#include "platform/systemtray.h"

#include <QApplication>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <algorithm>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QStandardPaths>
#include <QTimer>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QMessageBox>
#include <QProcessEnvironment>
#include <QStatusBar>

VauchiWindow::VauchiWindow(QWidget *parent) : QMainWindow(parent) {
    setWindowTitle(tr_vauchi("app.name", "Vauchi"));
    resize(700, 600);

    // Apply core theme colors via QPalette (runtime-switchable).
    //
    // VAUCHI_THEME=light selects the bundled Catppuccin Latte palette;
    // anything else (or unset) keeps the Catppuccin Mocha default.
    // The hook exists so the dark-mode snapshot parity test can launch
    // two qvauchi processes with identical inputs but opposite themes.
    // The env var is also a stable handle for any future user-mode
    // theme switching that needs to bootstrap before settings load.
    const QString themeChoice =
        QProcessEnvironment::systemEnvironment().value("VAUCHI_THEME").toLower();
    if (themeChoice == QStringLiteral("light")) {
        ThemeManager::applyDefaultLightTheme();
    } else {
        ThemeManager::applyDefaultTheme();
    }

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

    // Core's initial generic command batch decides whether the first
    // surface is Onboarding, Lock, or MyInfo. The shell does not select
    // or override that route.

    auto *central = new QWidget(this);
    auto *layout = new QHBoxLayout(central);
    m_presentation = new PresentationController(m_app, this);
    layout->addWidget(m_presentation, 1);

    setCentralWidget(central);
    connect(m_presentation, &PresentationController::nativeBackRequested,
            qApp, &QApplication::quit);
    connect(m_presentation, &PresentationController::wakeupScheduled, this,
            &VauchiWindow::scheduleWakeup);

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
                        window->m_presentation->refresh();
                        window->drainAndShowNotifications();
                    },
                    Qt::QueuedConnection);
            },
            this);

    }

    // Foreground hook: re-fetch the current screen on
    // background→foreground transition. Listener events cover most
    // state changes during backgrounding, but a missed event would
    // leave the UI stale until the next user action.
    connect(qApp, &QApplication::applicationStateChanged, this,
            [this](Qt::ApplicationState state) {
                if (state == Qt::ApplicationActive && m_presentation) {
                    m_presentation->refresh();
                    drainAndShowNotifications();
                }
            });

    // Core-driven wakeup tick (ADR-044 Am2a). Replaces any frontend-owned
    // poll loop with on_wakeup(); core decides when work is due and emits
    // the next ScheduleWakeup command.
    m_wakeupTimer = new QTimer(this);
    m_wakeupTimer->setSingleShot(true);
    connect(m_wakeupTimer, &QTimer::timeout, this, &VauchiWindow::onWakeup);
    onWakeup(); // bootstrap the schedule

    m_presentation->initialize();
}

void VauchiWindow::changeEvent(QEvent *event) {
    QMainWindow::changeEvent(event);
    if (event->type() == QEvent::WindowDeactivate && m_app) {
        m_presentation->appBackgrounded();
    }
}

VauchiWindow::~VauchiWindow() {
    if (m_app) {
        vauchi_app_destroy(m_app);
    }
}

void VauchiWindow::drainAndShowNotifications() {
    if (!m_app || !m_tray) return;

    char *json = vauchi_app_drain_notifications(m_app);
    if (!json) return;

    QJsonArray notifications = QJsonDocument::fromJson(json).array();
    vauchi_string_free(json);
    drainAndShowNotificationsArray(notifications);
}

void VauchiWindow::drainAndShowNotificationsArray(const QJsonArray &notifications) {
    if (!m_tray) return;

    for (const auto &n : notifications) {
        // TODO(HUMBLE): D — frontend maps notification category "EmergencyAlert" to OS tray icon severity; core should supply generic urgency hint (see _private/docs/problems/2026-07-06-desktop-tui-web-domain-shell-violations)
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

void VauchiWindow::keyPressEvent(QKeyEvent *event) {
    // Escape is the desktop system-back gesture. Forward it as a generic
    // BackRequested event; Core owns the pop-or-stop decision and returns
    // PerformNativeBack when there is nothing to pop.
    if (event->key() == Qt::Key_Escape && !(event->modifiers() & Qt::AltModifier)) {
        if (m_presentation) {
            m_presentation->requestBack();
        }
        event->accept();
        return;
    }
    QMainWindow::keyPressEvent(event);
}

void VauchiWindow::resizeEvent(QResizeEvent *event) {
    QMainWindow::resizeEvent(event);
    if (m_presentation) {
        m_presentation->reportEnvironment(
            m_presentation->width(), m_presentation->height());
    }
}

void VauchiWindow::scheduleWakeup(uint32_t seconds) {
    if (!m_wakeupTimer) return;
    // Cap at a sensible maximum to avoid a stray huge value stalling the loop.
    constexpr uint32_t maxSeconds = 3600;
    m_wakeupTimer->start(static_cast<int>(std::min(seconds, maxSeconds)) * 1000);
}

void VauchiWindow::onWakeup() {
    if (!m_app) return;

    char *json = vauchi_app_on_wakeup(m_app);
    if (!json) return;

    QJsonDocument doc = QJsonDocument::fromJson(json);
    vauchi_string_free(json);
    if (!doc.isObject()) return;

    QJsonObject envelope = doc.object();
    QJsonArray notifications = envelope["notifications"].toArray();
    QJsonArray commands = envelope["commands"].toArray();

    drainAndShowNotificationsArray(notifications);

    QJsonArray hardwareCommands;
    uint32_t nextWakeupSeconds = 30; // default fallback if no ScheduleWakeup
    for (const auto &cmd : commands) {
        if (cmd.isObject() && cmd.toObject().contains("ScheduleWakeup")) {
            QJsonObject sched = cmd.toObject()["ScheduleWakeup"].toObject();
            // Use deadline_secs as the next fire point; the shell only needs
            // an interval, and deadline is the conservative choice.
            nextWakeupSeconds = static_cast<uint32_t>(sched["deadline_secs"].toInt(30));
            continue;
        }
        hardwareCommands.append(cmd);
    }

    if (!hardwareCommands.isEmpty() && m_presentation) {
        m_presentation->dispatchCommands(hardwareCommands);
    }

    scheduleWakeup(nextWakeupSeconds);
}

void VauchiWindow::importContactsFromFile() {
    if (!m_app) return;

    QString path = QFileDialog::getOpenFileName(
        this,
        tr_vauchi("contacts.importContacts", "Import Contacts"),
        QString(),
        tr_vauchi("contacts.import_file_filter", "vCard Files (*.vcf)"));
    if (path.isEmpty()) return;

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        statusBar()->showMessage(
            tr_vauchi("platform.error_could_not_read_file",
                      "Could not read file"),
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
                      "Import failed"),
            4000);
        return;
    }

    QJsonObject obj = QJsonDocument::fromJson(result).object();
    vauchi_string_free(result);

    // TODO(HUMBLE): T — frontend assembles contact-import result messages from imported/skipped/warnings; core should return localized summary or Banner/Toast component (see _private/docs/problems/2026-07-06-desktop-tui-web-domain-shell-violations)
    if (obj.contains("error")) {
        statusBar()->showMessage(
            tr_vauchi("platform.error_import_failed_with_reason",
                      "Import failed: {error}")
            .replace(QStringLiteral("{error}"), obj["error"].toString()),
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
    m_presentation->refresh();
}
