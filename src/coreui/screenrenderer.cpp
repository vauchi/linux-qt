// SPDX-FileCopyrightText: 2026 Mattia Egloff <mattia.egloff@pm.me>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "screenrenderer.h"
#include "componentrenderer.h"
#include "thememanager.h"
#include "../platform/hardwarebackend.h"
#include "../i18n.h"

#include <QApplication>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QJsonDocument>
#include <QJsonArray>
#include <QMessageBox>
#include <QDesktopServices>
#include <QUrl>
#include <QInputDialog>
#include <QStatusBar>
#include <QMainWindow>
#include <QTimer>
#include <QFileDialog>
#include <QDateTime>
#include <QDir>
#include <QFile>

ScreenRenderer::ScreenRenderer(struct VauchiApp *app, QWidget *parent)
    : QWidget(parent), m_app(app) {
    m_layout = new QVBoxLayout(this);

    // A null app is the deliberately inert fixture-rendering mode. It builds
    // the production widget tree without starting core persistence, relay, or
    // hardware services. The live app always supplies a non-null handle.
    if (m_app) {
        m_hardware = new HardwareBackend(app, this);
        connect(m_hardware, &HardwareBackend::actionResultReady, this, [this](const QJsonObject &result) {
            QByteArray json = QJsonDocument(result).toJson(QJsonDocument::Compact);
            processActionResult(json.constData());
        });
        connect(m_hardware, &HardwareBackend::qrScanned, this, [this](const QString &data) {
            if (data.isEmpty()) {
                // No camera or scan cancelled — fall back to paste dialog
                promptQrPaste();
            } else {
                // Camera decoded QR — send as hardware event
                QJsonObject event;
                QJsonObject inner;
                inner["data"] = data;
                event["QrScanned"] = inner;
                QByteArray json = QJsonDocument(event).toJson(QJsonDocument::Compact);
                char *result = vauchi_app_handle_hardware_event(m_app, json.constData());
                if (result) {
                    processActionResult(result);
                    vauchi_string_free(result);
                } else {
                    refresh();
                }
            }
        });

        refresh();
    }
}

void ScreenRenderer::refresh() {
    if (!m_app) return;

    char *jsonStr = vauchi_app_current_screen(m_app);
    if (!jsonStr) return;

    QJsonDocument doc = QJsonDocument::fromJson(jsonStr);
    vauchi_string_free(jsonStr);

    if (doc.isObject()) {
        renderScreen(doc.object());
    }
}

void ScreenRenderer::renderFixture(const QJsonObject &screen) {
    renderScreen(screen);
}

static void clearLayout(QLayout *layout) {
    QLayoutItem *item;
    while ((item = layout->takeAt(0)) != nullptr) {
        if (QWidget *w = item->widget()) {
            w->deleteLater();
        } else if (QLayout *child = item->layout()) {
            clearLayout(child);
        }
        delete item;
    }
}

void ScreenRenderer::renderScreen(const QJsonObject &screen) {
    // Clear existing widgets and nested layouts
    clearLayout(m_layout);

    // Top/leading chrome actions (ADR-044 Am2a). Core owns which chrome
    // affordances exist; the frontend renders them without interpreting the
    // screen role. The reserved `go_back` id dispatches the system-back
    // `UserAction` so the visible button and the Escape gesture share one
    // code path.
    QJsonArray navActions = screen["nav_actions"].toArray();
    if (!navActions.isEmpty()) {
        auto *navLayout = new QHBoxLayout;
        for (const auto &nav : navActions) {
            QJsonObject navObj = nav.toObject();
            QString navId = navObj["id"].toString();
            QString label = navObj["label"].toString();
            if (navId == QStringLiteral("go_back")) {
                label = QStringLiteral("‹ ") + label;
            }
            auto *btn = new QPushButton(label);
            btn->setEnabled(navObj["enabled"].toBool(true));
            btn->setObjectName(navId);
            btn->setFlat(true);

            QJsonObject a11y = navObj["a11y"].toObject();
            if (a11y.contains("label")) {
                btn->setAccessibleName(a11y["label"].toString());
            }
            if (a11y.contains("hint")) {
                btn->setAccessibleDescription(a11y["hint"].toString());
            }

            connect(btn, &QPushButton::clicked, this, [this, navId]() {
                if (navId == QStringLiteral("go_back")) {
                    dispatchNavigateBack();
                } else {
                    handleNavAction(navId);
                }
            });

            navLayout->addWidget(btn);
        }
        navLayout->addStretch();
        m_layout->addLayout(navLayout);
    }

    auto *title = new QLabel(screen["title"].toString());
    title->setStyleSheet(ThemeManager::fontFamilyCss() +
                         QStringLiteral("font-size: 24px; font-weight: bold;"));
    title->setObjectName(QStringLiteral("screen_title"));
    title->setAccessibleName(screen["title"].toString());
    m_layout->addWidget(title);

    if (screen.contains("subtitle") && !screen["subtitle"].isNull()) {
        auto *subtitle = new QLabel(screen["subtitle"].toString());
        subtitle->setAccessibleName(screen["subtitle"].toString());
        subtitle->setStyleSheet(ThemeManager::styleForRole(ThemeRole::SecondaryText));
        m_layout->addWidget(subtitle);
    }

    // Components — pass unified action callback
    auto onAction = [this](const QJsonObject &action) {
        handleComponentAction(action);
    };
    QJsonArray components = screen["components"].toArray();
    for (const auto &comp : components) {
        QWidget *widget = ComponentRenderer::render(comp.toObject(), onAction);
        if (widget) {
            m_layout->addWidget(widget);
        }
    }

    // Action buttons
    m_buttons.clear();
    auto *buttonLayout = new QHBoxLayout;
    QJsonArray actions = screen["actions"].toArray();
    for (const auto &action : actions) {
        QJsonObject actionObj = action.toObject();
        auto *btn = new QPushButton(actionObj["label"].toString());
        btn->setEnabled(actionObj["enabled"].toBool(true));

        QString actionId = actionObj["id"].toString();
        // `objectName` is the stable id test drivers (AT-SPI, Qt
        // Test, etc.) look up. Qt derives the accessible name from
        // the button's visible text by default; core-provided
        // `a11y` overrides below.
        btn->setObjectName(actionId);

        // Core-provided accessibility override (plan Task 3.1 /
        // `_private/docs/problems/2026-04-20-screen-action-a11y-identifier-gap`).
        // `a11y.label` replaces the visible-text-derived screen-
        // reader announcement; `a11y.hint` maps to Qt's accessible
        // description. When `a11y` is absent, Qt keeps its default
        // (label-derived) behaviour — no visible change.
        QJsonObject a11y = actionObj["a11y"].toObject();
        if (a11y.contains("label")) {
            btn->setAccessibleName(a11y["label"].toString());
        }
        if (a11y.contains("hint")) {
            btn->setAccessibleDescription(a11y["hint"].toString());
        }

        m_buttons.append({actionId, btn});
        connect(btn, &QPushButton::clicked, this, [this, actionId]() {
            handleAction(actionId);
        });

        buttonLayout->addWidget(btn);
    }
    m_layout->addLayout(buttonLayout);
    m_layout->addStretch();
}

void ScreenRenderer::handleComponentAction(const QJsonObject &action) {
    if (!m_app) return;

    QByteArray json = QJsonDocument(action).toJson(QJsonDocument::Compact);
    char *result = vauchi_app_handle_action(m_app, json.constData());
    if (result) {
        processActionResult(result);
        vauchi_string_free(result);
    }

    // Update button enabled states without rebuilding the widget tree
    updateButtonStates();
}

void ScreenRenderer::dispatchNavigateBack() {
    if (!m_app) return;
    // ADR-044 Am2a: forward the OS back gesture as a typed UserAction.
    // Core owns the pop-or-stop decision and returns PerformNativeBack
    // when there is nothing to pop.
    QByteArray json = QStringLiteral("\"NavigateBack\"").toUtf8();
    char *result = vauchi_app_handle_action(m_app, json.constData());
    if (result) {
        processActionResult(result);
        vauchi_string_free(result);
    }
}

void ScreenRenderer::handleNavAction(const QString &actionId) {
    // Non-`go_back` nav actions (e.g. `open_settings`) use the same
    // ActionPressed lane as per-screen actions so core can resolve them.
    handleAction(actionId);
}

void ScreenRenderer::dispatchCommands(const QJsonArray &commands) {
    QJsonObject wrapper;
    QJsonObject inner;
    inner["commands"] = commands;
    wrapper["Commands"] = inner;
    QByteArray json = QJsonDocument(wrapper).toJson(QJsonDocument::Compact);
    processActionResult(json.constData());
}

void ScreenRenderer::processActionResult(const char *resultJson) {
    QJsonDocument doc = QJsonDocument::fromJson(resultJson);
    if (!doc.isObject()) {
        refresh();
        return;
    }

    QJsonObject result = doc.object();

    if (result.contains("NavigateTo") || result.contains("UpdateScreen")) {
        // Core already updated internal state; refresh to display the new screen
        refresh();
        emit screenChanged();
    } else if (result.contains("PerformNativeBack")) {
        // ADR-044 Am2a: back reached a back-stopping root. Desktop's native
        // default is to exit the application.
        QApplication::quit();
    } else if (result.contains("ShowAlert")) {
        QJsonObject alert = result["ShowAlert"].toObject();
        QMessageBox::information(this, alert["title"].toString(), alert["message"].toString());
    } else if (result.contains("OpenUrl")) {
        QUrl url(result["OpenUrl"].toObject()["url"].toString());
        if (url.isValid() && (url.scheme() == "http" || url.scheme() == "https")) {
            QDesktopServices::openUrl(url);
        } else if (!url.isEmpty()) {
            QMessageBox::warning(this,
                                 tr_vauchi("platform.error_could_not_open_link",
                                           "Cannot open link"),
                                 tr_vauchi("platform.error_url_scheme_not_allowed",
                                           "URL scheme not allowed: {url}")
                                           .replace(QStringLiteral("{url}"), url.scheme()));
        }
    } else if (result.contains("WipeComplete")) {
        refresh();
        emit screenChanged();
    } else if (result.contains("Commands")) {
        // ADR-031: Dispatch exchange commands to hardware backends.
        // Camera/BLE/NFC backends handle their respective commands;
        // unavailable hardware triggers HardwareUnavailable events back to core.
        QJsonObject cmds = result["Commands"].toObject();
        QJsonArray commands = cmds["commands"].toArray();
        refresh();
        if (m_hardware) m_hardware->dispatchCommands(commands);
    } else if (result.contains("ShowToast")) {
        QJsonObject toast = result["ShowToast"].toObject();
        QString message = toast["message"].toString();
        QString undoId = toast["undo_action_id"].toString();
        QString undoLabel = toast["undo_label"].toString();
        if (!undoId.isEmpty() && !undoLabel.isEmpty()) {
            showStatusMessageWithUndo(message, undoId, undoLabel);
        } else {
            showStatusMessage(message);
        }
        refresh();
    } else if (result.contains("BackupExportComplete")) {
        QJsonObject backup = result["BackupExportComplete"].toObject();
        QString hexData = backup["data"].toString();
        saveBackupToFile(hexData);
        refresh();
        emit screenChanged();
    } else if (result.contains("Complete")) {
        refresh();
        emit screenChanged();
    } else if (result.contains("ValidationError")) {
        QJsonObject err = result["ValidationError"].toObject();
        QString componentId = err["component_id"].toString();
        QString message = err["message"].toString();
        showValidationError(componentId, message);
    } else {
        // Navigation actions resolved to NavigateTo by core (`route_result`)
        // before reaching the frontend — OpenContact / EditContact /
        // OpenEntryDetail / etc. never arrive raw (ADR-043 Humble UI).
        refresh();
    }
}

void ScreenRenderer::updateButtonStates() {
    if (!m_app) return;

    char *jsonStr = vauchi_app_current_screen(m_app);
    if (!jsonStr) return;

    QJsonDocument doc = QJsonDocument::fromJson(jsonStr);
    vauchi_string_free(jsonStr);

    QJsonArray actions = doc.object()["actions"].toArray();
    for (const auto &action : actions) {
        QJsonObject actionObj = action.toObject();
        QString id = actionObj["id"].toString();
        bool enabled = actionObj["enabled"].toBool(true);
        for (auto &pair : m_buttons) {
            if (pair.first == id && pair.second) {
                pair.second->setEnabled(enabled);
            }
        }
    }
}

void ScreenRenderer::showValidationError(const QString &componentId,
                                          const QString &message) {
    // Find the target widget by objectName (set during component rendering)
    QWidget *target = findChild<QWidget *>(componentId);
    if (!target) {
        // Fallback: show as status bar message
        showStatusMessage(message);
        return;
    }

    // Add themed error border to the target widget
    QString original = target->styleSheet();
    target->setStyleSheet(original + QStringLiteral("; ") +
                          ThemeManager::styleForRole(ThemeRole::ErrorBorder));

    // Insert error label below the target widget
    auto *errorLabel = new QLabel(message, this);
    errorLabel->setObjectName(componentId + "_error");
    errorLabel->setStyleSheet(ThemeManager::fontFamilyCss() +
                              ThemeManager::styleForRole(ThemeRole::DestructiveText) +
                              QStringLiteral(" font-size: 12px;"));

    // Find the target's position in the layout and insert after it
    for (int i = 0; i < m_layout->count(); ++i) {
        if (m_layout->itemAt(i)->widget() == target) {
            m_layout->insertWidget(i + 1, errorLabel);
            break;
        }
    }

    // Also show in status bar for visibility
    showStatusMessage(message);
}

void ScreenRenderer::promptQrPaste() {
    bool ok = false;
    QString data = QInputDialog::getText(
        this, tr_vauchi("platform.qr_scan_title", "Scan QR Code"),
        tr_vauchi("platform.qr_paste_peer_data",
                  "Paste the peer's QR code data:"),
        QLineEdit::Normal, QString(), &ok);
    if (ok && !data.isEmpty()) {
        // Send as TextChanged action with component_id "scanned_data"
        // TODO(HUMBLE): W — QR-paste fallback hardcodes target component id "scanned_data"; core should include target component_id in RequestCamera result or dedicated QrPasted event (see _private/docs/problems/2026-07-06-desktop-tui-web-domain-shell-violations)
        QJsonObject action;
        QJsonObject inner;
        inner["component_id"] = QStringLiteral("scanned_data");
        inner["value"] = data;
        action["TextChanged"] = inner;
        handleComponentAction(action);
    }
}

void ScreenRenderer::showStatusMessage(const QString &message) {
    // Find the parent QMainWindow's status bar
    QWidget *w = parentWidget();
    while (w) {
        if (auto *mw = qobject_cast<QMainWindow *>(w)) {
            mw->statusBar()->showMessage(message, 4000);
            return;
        }
        w = w->parentWidget();
    }
}

void ScreenRenderer::showStatusMessageWithUndo(
    const QString &message, const QString &undoActionId,
    const QString &undoLabel) {
    QWidget *w = parentWidget();
    while (w) {
        if (auto *mw = qobject_cast<QMainWindow *>(w)) {
            auto *bar = mw->statusBar();
            bar->showMessage(message);
            auto *btn = new QPushButton(undoLabel, bar);
            bar->addPermanentWidget(btn);
            connect(btn, &QPushButton::clicked, this,
                    [this, btn, undoActionId]() {
                        btn->deleteLater();
                        handleUndoAction(undoActionId);
                    });
            // Auto-dismiss after 4 seconds
            QTimer::singleShot(4000, btn, [btn, bar]() {
                bar->clearMessage();
                btn->deleteLater();
            });
            return;
        }
        w = w->parentWidget();
    }
}

void ScreenRenderer::handleAction(const QString &actionId) {
    if (!m_app) return;

    QJsonObject action;
    QJsonObject inner;
    inner["action_id"] = actionId;
    action["ActionPressed"] = inner;

    QByteArray json = QJsonDocument(action).toJson(QJsonDocument::Compact);
    char *result = vauchi_app_handle_action(m_app, json.constData());
    if (result) {
        processActionResult(result);
        vauchi_string_free(result);
        return;
    }

    refresh();
    emit screenChanged();
}

void ScreenRenderer::handleUndoAction(const QString &actionId) {
    if (!m_app) return;

    QJsonObject action;
    QJsonObject inner;
    inner["action_id"] = actionId;
    action["UndoPressed"] = inner;

    QByteArray json = QJsonDocument(action).toJson(QJsonDocument::Compact);
    char *result = vauchi_app_handle_action(m_app, json.constData());
    if (result) {
        processActionResult(result);
        vauchi_string_free(result);
        return;
    }

    refresh();
    emit screenChanged();
}

void ScreenRenderer::saveBackupToFile(const QString &hexData) {
    // TODO(HUMBLE): W — backup export dialog hardcodes filename pattern and file filter; core should provide suggested_filename and file_filter (see _private/docs/problems/2026-07-06-desktop-tui-web-domain-shell-violations)
    QString defaultName = QStringLiteral("vauchi-backup-%1.vbk")
        .arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd")));

    QString path = QFileDialog::getSaveFileName(
        this,
        tr_vauchi("backup.save_title", "Save Backup"),
        QDir::homePath() + QStringLiteral("/") + defaultName,
        tr_vauchi("backup.file_filter", "Vauchi Backup (*.vbk)"));

    if (path.isEmpty()) return;

    QByteArray raw = QByteArray::fromHex(hexData.toLatin1());
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        showStatusMessage(
            tr_vauchi("platform.error_could_not_write_file",
                      "Could not save backup file"));
        return;
    }
    file.write(raw);
    file.close();

    showStatusMessage(
        tr_vauchi("backup.export_success",
                  "Backup saved successfully"));
}
