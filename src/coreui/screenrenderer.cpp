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

ScreenRenderer::ScreenRenderer(VauchiWorkflow *workflow, QWidget *parent)
    : QWidget(parent), m_workflow(workflow) {
    m_layout = new QVBoxLayout(this);
    refresh();
}

void ScreenRenderer::refresh() {
    if (!m_workflow) return;

    char *jsonStr = vauchi_workflow_current_screen(m_workflow);
    if (!jsonStr) return;

    QJsonDocument doc = QJsonDocument::fromJson(jsonStr);
    vauchi_string_free(jsonStr);

    if (doc.isObject()) {
        renderScreen(doc.object());
    }
}

void ScreenRenderer::renderScreen(const QJsonObject &screen) {
    // Clear existing widgets
    QLayoutItem *item;
    while ((item = m_layout->takeAt(0)) != nullptr) {
        delete item->widget();
        delete item;
    }

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

    // Components
    QJsonArray components = screen["components"].toArray();
    for (const auto &comp : components) {
        QWidget *widget = ComponentRenderer::render(comp.toObject());
        if (widget) {
            m_layout->addWidget(widget);
        }
    }

    // Action buttons
    auto *buttonLayout = new QHBoxLayout;
    QJsonArray actions = screen["actions"].toArray();
    for (const auto &action : actions) {
        QJsonObject actionObj = action.toObject();
        auto *btn = new QPushButton(actionObj["label"].toString());
        btn->setEnabled(actionObj["enabled"].toBool(true));

        QString actionId = actionObj["id"].toString();
        connect(btn, &QPushButton::clicked, this, [this, actionId]() {
            handleAction(actionId);
        });

        buttonLayout->addWidget(btn);
    }
    m_layout->addLayout(buttonLayout);
    m_layout->addStretch();
}

void ScreenRenderer::handleAction(const QString &actionId) {
    QJsonObject action;
    QJsonObject inner;
    inner["action_id"] = actionId;
    action["ActionPressed"] = inner;

    QByteArray json = QJsonDocument(action).toJson(QJsonDocument::Compact);
    char *result = vauchi_workflow_handle_action(m_workflow, json.constData());
    if (result) {
        vauchi_string_free(result);
    }

    refresh();
}
