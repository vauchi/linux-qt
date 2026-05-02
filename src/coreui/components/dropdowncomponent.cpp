// SPDX-FileCopyrightText: 2026 Mattia Egloff <mattia.egloff@pm.me>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "dropdowncomponent.h"
#include <QComboBox>
#include <QJsonArray>
#include <QLabel>
#include <QVBoxLayout>

QWidget *DropdownComponent::render(const QJsonObject &data,
                                   const OnAction &onAction) {
    auto *container = new QWidget;
    auto *layout = new QVBoxLayout(container);
    QString componentId = data["id"].toString();
    container->setObjectName(componentId);

    QString label = data["label"].toString();
    if (!label.isEmpty()) {
        layout->addWidget(new QLabel(label));
    }

    auto *combo = new QComboBox;
    combo->setAccessibleName(label);

    QJsonArray options = data["options"].toArray();
    QStringList optionIds;
    for (const auto &opt : options) {
        QJsonObject o = opt.toObject();
        optionIds << o["id"].toString();
        combo->addItem(o["label"].toString());
    }

    QString selected = data["selected"].toString();
    if (!selected.isEmpty()) {
        int idx = optionIds.indexOf(selected);
        if (idx >= 0) combo->setCurrentIndex(idx);
    }

    if (data.contains("a11y") && data["a11y"].isObject()) {
        auto a11y = data["a11y"].toObject();
        auto a11yLabel = a11y.value("label").toString();
        if (!a11yLabel.isEmpty()) combo->setAccessibleName(a11yLabel);
    }

    if (onAction) {
        QObject::connect(
            combo, QOverload<int>::of(&QComboBox::currentIndexChanged), combo,
            [onAction, componentId, optionIds](int index) {
                if (index < 0 || index >= optionIds.size()) return;
                QJsonObject action;
                QJsonObject inner;
                inner["component_id"] = componentId;
                inner["item_id"] = optionIds.at(index);
                action["ListItemSelected"] = inner;
                onAction(action);
            });
    }

    layout->addWidget(combo);
    return container;
}
