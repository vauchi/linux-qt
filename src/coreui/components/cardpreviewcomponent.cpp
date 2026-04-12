// SPDX-FileCopyrightText: 2026 Mattia Egloff <mattia.egloff@pm.me>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "cardpreviewcomponent.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QFrame>
#include <QJsonArray>
#include <QTabWidget>

QWidget *CardPreviewComponent::render(const QJsonObject &data,
                                      const OnAction &onAction) {
    auto *frame = new QFrame;
    frame->setFrameStyle(QFrame::Box | QFrame::Raised);
    frame->setObjectName(QStringLiteral("card_preview"));
    frame->setAccessibleName("Contact card: " + data["name"].toString());
    auto *layout = new QVBoxLayout(frame);

    auto *name = new QLabel(data["name"].toString());
    name->setStyleSheet("font-size: 18px; font-weight: bold;");
    name->setAccessibleName(data["name"].toString());
    layout->addWidget(name);

    // Render top-level fields
    QJsonArray fields = data["fields"].toArray();
    for (const auto &field : fields) {
        QJsonObject f = field.toObject();
        auto *row = new QHBoxLayout;
        auto *label = new QLabel(f["label"].toString() + ":");
        label->setStyleSheet("font-weight: bold;");
        auto *value = new QLabel(f["value"].toString());
        value->setWordWrap(true);
        row->addWidget(label);
        row->addWidget(value, 1);
        layout->addLayout(row);
    }

    // Render group views as tabs if present
    QJsonArray groups = data["group_views"].toArray();
    if (!groups.isEmpty()) {
        auto *tabs = new QTabWidget;
        for (const auto &group : groups) {
            QJsonObject g = group.toObject();
            auto *page = new QWidget;
            auto *pageLayout = new QVBoxLayout(page);
            QJsonArray gFields = g["visible_fields"].toArray();
            for (const auto &gf : gFields) {
                QJsonObject f = gf.toObject();
                auto *row = new QHBoxLayout;
                auto *label = new QLabel(f["label"].toString() + ":");
                label->setStyleSheet("font-weight: bold;");
                auto *value = new QLabel(f["value"].toString());
                value->setWordWrap(true);
                row->addWidget(label);
                row->addWidget(value, 1);
                pageLayout->addLayout(row);
            }
            pageLayout->addStretch();
            tabs->addTab(page, g["display_name"].toString());
        }
        // Select the active group if specified
        QString selectedGroup = data["selected_group"].toString();
        if (!selectedGroup.isEmpty()) {
            for (int i = 0; i < groups.size(); ++i) {
                if (groups[i].toObject()["group_name"].toString() == selectedGroup) {
                    tabs->setCurrentIndex(i);
                    break;
                }
            }
        }
        // Wire tab switch to emit GroupViewSelected
        if (onAction) {
            QObject::connect(tabs, &QTabWidget::currentChanged, tabs,
                             [onAction, groups](int index) {
                                 if (index < 0 || index >= groups.size()) return;
                                 QString groupName = groups[index].toObject()["group_name"].toString();
                                 QJsonObject action;
                                 QJsonObject inner;
                                 inner["group_name"] = groupName;
                                 action["GroupViewSelected"] = inner;
                                 onAction(action);
                             });
        }

        layout->addWidget(tabs);
    }

    if (data.contains("a11y") && data["a11y"].isObject()) {
        auto a11y = data["a11y"].toObject();
        auto a11yLabel = a11y.value("label").toString();
        if (!a11yLabel.isEmpty()) {
            frame->setAccessibleName(a11yLabel);
        }
        auto hint = a11y.value("hint").toString();
        if (!hint.isEmpty()) {
            frame->setAccessibleDescription(hint);
        }
    }

    return frame;
}
