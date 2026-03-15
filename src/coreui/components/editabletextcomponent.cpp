// SPDX-FileCopyrightText: 2026 Mattia Egloff <mattia.egloff@pm.me>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "editabletextcomponent.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>

QWidget *EditableTextComponent::render(const QJsonObject &data,
                                       const OnAction &onAction) {
    auto *container = new QWidget;
    auto *layout = new QVBoxLayout(container);
    layout->setContentsMargins(0, 0, 0, 0);

    QString componentId = data["id"].toString();
    QString labelText = data["label"].toString();
    QString value = data["value"].toString();
    bool editing = data["editing"].toBool();

    container->setObjectName(componentId);
    container->setAccessibleName(labelText);

    // Label
    auto *label = new QLabel(labelText);
    label->setStyleSheet("font-size: 12px; color: #888;");
    layout->addWidget(label);

    if (editing) {
        // Edit mode: text entry + save button
        auto *editRow = new QHBoxLayout;

        auto *input = new QLineEdit(value);
        input->setAccessibleName(labelText);
        input->setObjectName(componentId + "_input");
        editRow->addWidget(input, 1);

        auto *saveBtn = new QPushButton(QStringLiteral("Save"));
        saveBtn->setStyleSheet("color: white; background-color: #3498db; border-radius: 4px; padding: 4px 12px;");
        editRow->addWidget(saveBtn);

        if (onAction) {
            // Save button: emit TextChanged then ActionPressed
            QObject::connect(saveBtn, &QPushButton::clicked, saveBtn,
                             [onAction, componentId, input]() {
                                 QJsonObject textAction;
                                 QJsonObject textInner;
                                 textInner["component_id"] = componentId;
                                 textInner["value"] = input->text();
                                 textAction["TextChanged"] = textInner;
                                 onAction(textAction);

                                 QJsonObject pressAction;
                                 QJsonObject pressInner;
                                 pressInner["action_id"] = componentId + "_save";
                                 pressAction["ActionPressed"] = pressInner;
                                 onAction(pressAction);
                             });

            // Enter key: same as Save
            QObject::connect(input, &QLineEdit::returnPressed, input,
                             [onAction, componentId, input]() {
                                 QJsonObject textAction;
                                 QJsonObject textInner;
                                 textInner["component_id"] = componentId;
                                 textInner["value"] = input->text();
                                 textAction["TextChanged"] = textInner;
                                 onAction(textAction);

                                 QJsonObject pressAction;
                                 QJsonObject pressInner;
                                 pressInner["action_id"] = componentId + "_save";
                                 pressAction["ActionPressed"] = pressInner;
                                 onAction(pressAction);
                             });
        }

        layout->addLayout(editRow);

        // Validation error
        QString error = data["validation_error"].toString();
        if (!error.isEmpty()) {
            auto *errLabel = new QLabel(error);
            errLabel->setStyleSheet("color: red;");
            layout->addWidget(errLabel);
        }
    } else {
        // Display mode: value text + edit button
        auto *displayRow = new QHBoxLayout;

        auto *valueLabel = new QLabel(value);
        displayRow->addWidget(valueLabel, 1);

        auto *editBtn = new QPushButton(QStringLiteral("Edit"));
        editBtn->setFlat(true);
        displayRow->addWidget(editBtn);

        if (onAction) {
            QObject::connect(editBtn, &QPushButton::clicked, editBtn,
                             [onAction, componentId]() {
                                 QJsonObject action;
                                 QJsonObject inner;
                                 inner["action_id"] = componentId + "_edit";
                                 action["ActionPressed"] = inner;
                                 onAction(action);
                             });
        }

        layout->addLayout(displayRow);
    }

    return container;
}
