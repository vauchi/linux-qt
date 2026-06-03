// SPDX-FileCopyrightText: 2026 Mattia Egloff <mattia.egloff@pm.me>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "rowcomponent.h"
#include <QHBoxLayout>
#include <QJsonArray>

QWidget *RowComponent::render(const QJsonObject &data,
                              const OnAction &onAction) {
    auto *container = new QWidget;
    container->setObjectName(data["id"].toString());

    auto *layout = new QHBoxLayout(container);
    layout->setContentsMargins(0, 0, 0, 0);

    // Render each child via the shared dispatcher (recursive — a Row may
    // contain any Component, including another Row). Every child gets
    // equal stretch so each is width-bounded: a child that fills its own
    // max width then fills only its weighted slice instead of overflowing
    // and overlapping its siblings (mirrors android ScreenRenderer's
    // `Box(Modifier.weight(1f))` per child).
    QJsonArray items = data["items"].toArray();
    for (const auto &entry : items) {
        QWidget *child = ComponentRenderer::render(entry.toObject(), onAction);
        if (child) {
            layout->addWidget(child, /*stretch=*/1);
        }
    }

    return container;
}
