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

    if (result.contains("NavigateTo")) {
        QString screen = result["NavigateTo"].toObject()["screen"].toString();
        if (!screen.isEmpty()) {
            QByteArray screenUtf8 = screen.toUtf8();
            char *navResult = vauchi_app_navigate_to(m_app, screenUtf8.constData());
            if (navResult) {
                vauchi_string_free(navResult);
            }
        }
        refresh();
        emit screenChanged();
    } else if (result.contains("ShowAlert")) {
        QJsonObject alert = result["ShowAlert"].toObject();
        QString alertTitle = alert["title"].toString();
        QString alertMessage = alert["message"].toString();
        QMessageBox::information(this, alertTitle, alertMessage);
    } else if (result.contains("OpenUrl")) {
        QString url = result["OpenUrl"].toObject()["url"].toString();
        if (!url.isEmpty()) {
            QDesktopServices::openUrl(QUrl(url));
        }
    } else {
        // For Refresh, StateUpdated, or unknown variants — just refresh
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
            if (pair.first == id) {
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
