// SPDX-FileCopyrightText: 2026 Mattia Egloff <mattia.egloff@pm.me>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "screenrenderer.h"
#include "componentrenderer.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QJsonDocument>
#include <QJsonArray>
#include <QMessageBox>
#include <QDesktopServices>
#include <QUrl>

ScreenRenderer::ScreenRenderer(struct VauchiApp *app, QWidget *parent)
    : QWidget(parent), m_app(app) {
    m_layout = new QVBoxLayout(this);
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
    } else {
        // ValidationError, Complete, RequestCamera, OpenEntryDetail, etc.
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
