// SPDX-FileCopyrightText: 2026 Mattia Egloff <mattia.egloff@pm.me>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "fieldlistcomponent.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QCheckBox>
#include <QPushButton>
#include <QJsonArray>

QWidget *FieldListComponent::render(const QJsonObject &data,
                                     const OnAction &onAction) {
    auto *container = new QWidget;
    auto *layout = new QVBoxLayout(container);
    layout->setContentsMargins(0, 0, 0, 0);

    QString visibilityMode = data["visibility_mode"].toString();
    QJsonArray availableGroups = data["available_groups"].toArray();
    QJsonArray fields = data["fields"].toArray();

    for (const auto &field : fields) {
        QJsonObject fieldObj = field.toObject();
        QString fieldId = fieldObj["id"].toString();

        auto *row = new QHBoxLayout;

        // Field label and value
        auto *label = new QLabel(fieldObj["label"].toString() + ":");
        label->setStyleSheet("font-weight: bold;");
        auto *value = new QLabel(fieldObj["value"].toString());
        value->setWordWrap(true);
        row->addWidget(label);
        row->addWidget(value, 1);

        if (visibilityMode == "ShowHide") {
            // Simple show/hide toggle
            QJsonValue vis = fieldObj["visibility"];
            bool isVisible = vis.isString() && vis.toString() == "Shown";

            auto *btn = new QPushButton(isVisible ? "Visible" : "Hidden");
            btn->setFlat(true);
            if (onAction) {
                QObject::connect(btn, &QPushButton::clicked, btn,
                                 [onAction, fieldId, isVisible]() {
                                     QJsonObject action;
                                     QJsonObject inner;
                                     inner["field_id"] = fieldId;
                                     inner["visible"] = !isVisible;
                                     action["FieldVisibilityChanged"] = inner;
                                     onAction(action);
                                 });
            }
            row->addWidget(btn);
        } else if (visibilityMode == "PerGroup") {
            // Per-group checkboxes
            QJsonValue vis = fieldObj["visibility"];
            QStringList activeGroups;
            if (vis.isObject()) {
                QJsonArray groups = vis.toObject()["Groups"].toArray();
                for (const auto &g : groups) {
                    activeGroups.append(g.toString());
                }
            }

            for (const auto &group : availableGroups) {
                QString groupName = group.toString();
                bool isActive = activeGroups.contains(groupName);

                auto *check = new QCheckBox(groupName);
                check->setChecked(isActive);
                if (onAction) {
                    QObject::connect(check, &QCheckBox::toggled, check,
                                     [onAction, fieldId, groupName](bool checked) {
                                         QJsonObject action;
                                         QJsonObject inner;
                                         inner["field_id"] = fieldId;
                                         inner["group_id"] = groupName;
                                         inner["visible"] = checked;
                                         action["FieldVisibilityChanged"] = inner;
                                         onAction(action);
                                     });
                }
                row->addWidget(check);
            }
        }
        // ReadOnly: no visibility controls

        layout->addLayout(row);
    }

    return container;
}
