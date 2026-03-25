// SPDX-FileCopyrightText: 2026 Mattia Egloff <mattia.egloff@pm.me>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "screenrenderer.h"
#include "componentrenderer.h"
#include "../platform/hardwarebackend.h"

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

ScreenRenderer::ScreenRenderer(struct VauchiApp *app, QWidget *parent)
    : QWidget(parent), m_app(app) {
    m_layout = new QVBoxLayout(this);

    // Hardware backend for exchange commands
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

    // Title
    auto *title = new QLabel(screen["title"].toString());
    title->setStyleSheet("font-size: 24px; font-weight: bold;");
    title->setObjectName(QStringLiteral("screen_title"));
    title->setAccessibleName(screen["title"].toString());
    m_layout->addWidget(title);

    // Subtitle
    if (screen.contains("subtitle") && !screen["subtitle"].isNull()) {
        auto *subtitle = new QLabel(screen["subtitle"].toString());
        subtitle->setStyleSheet("color: gray;");
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
        btn->setObjectName(actionId);
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
    QByteArray json = QJsonDocument(action).toJson(QJsonDocument::Compact);
    char *result = vauchi_app_handle_action(m_app, json.constData());
    if (result) {
        processActionResult(result);
        vauchi_string_free(result);
    }

    // Update button enabled states without rebuilding the widget tree
    updateButtonStates();
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
    } else if (result.contains("ShowAlert")) {
        QJsonObject alert = result["ShowAlert"].toObject();
        QMessageBox::information(this, alert["title"].toString(), alert["message"].toString());
    } else if (result.contains("OpenUrl")) {
        QUrl url(result["OpenUrl"].toObject()["url"].toString());
        if (url.isValid() && (url.scheme() == "http" || url.scheme() == "https")) {
            QDesktopServices::openUrl(url);
        } else if (!url.isEmpty()) {
            QMessageBox::warning(this, tr("Cannot open link"),
                                 tr("URL scheme not allowed: %1").arg(url.scheme()));
        }
    } else if (result.contains("StartDeviceLink")) {
        char *r = vauchi_app_navigate_to(m_app, "device_linking");
        if (r) vauchi_string_free(r);
        refresh();
        emit screenChanged();
    } else if (result.contains("StartBackupImport")) {
        char *r = vauchi_app_navigate_to(m_app, "backup");
        if (r) vauchi_string_free(r);
        refresh();
        emit screenChanged();
    } else if (result.contains("OpenContact")) {
        // CABI doesn't support parameterized navigation yet — refresh as fallback
        refresh();
        emit screenChanged();
    } else if (result.contains("EditContact")) {
        // CABI doesn't support parameterized navigation yet — refresh as fallback
        refresh();
        emit screenChanged();
    } else if (result.contains("WipeComplete")) {
        refresh();
        emit screenChanged();
    } else if (result.contains("ExchangeCommands")) {
        // ADR-031: Dispatch exchange commands to hardware backends.
        // Camera/BLE/NFC backends handle their respective commands;
        // unavailable hardware triggers HardwareUnavailable events back to core.
        QJsonObject cmds = result["ExchangeCommands"].toObject();
        QJsonArray commands = cmds["commands"].toArray();
        refresh();
        m_hardware->dispatchCommands(commands);
    } else if (result.contains("RequestCamera")) {
        // Desktop fallback: prompt user to paste QR data manually
        refresh();
        promptQrPaste();
    } else if (result.contains("ShowToast")) {
        QJsonObject toast = result["ShowToast"].toObject();
        showStatusMessage(toast["message"].toString());
        refresh();
    } else if (result.contains("Complete")) {
        refresh();
        emit screenChanged();
    } else if (result.contains("ValidationError")) {
        QJsonObject err = result["ValidationError"].toObject();
        QString componentId = err["component_id"].toString();
        QString message = err["message"].toString();
        showValidationError(componentId, message);
    } else {
        // OpenEntryDetail, etc.
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

    // Add red border to the target widget
    QString original = target->styleSheet();
    target->setStyleSheet(original + QStringLiteral("; border: 2px solid red;"));

    // Insert error label below the target widget
    auto *errorLabel = new QLabel(message, this);
    errorLabel->setObjectName(componentId + "_error");
    errorLabel->setStyleSheet("color: red; font-size: 12px;");

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
        this, tr("Scan QR Code"),
        tr("Paste the peer's QR code data:"),
        QLineEdit::Normal, QString(), &ok);
    if (ok && !data.isEmpty()) {
        // Send as TextChanged action with component_id "scanned_data"
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

void ScreenRenderer::handleAction(const QString &actionId) {
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
