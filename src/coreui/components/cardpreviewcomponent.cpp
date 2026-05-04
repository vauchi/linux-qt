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

    // Render top-level fields — G1 (ADR-021/043): use core's pre-filtered
    // `visible_fields` list instead of the raw `fields` array. The previous
    // render of `data["fields"]` leaked Hidden-visibility fields into the
    // preview. `visible_fields` is computed by core's `build_visible_fields`
    // helper and respects both the per-group selection and the global
    // Shown/Groups/Hidden filter.
    QJsonArray visibleFields = data["visible_fields"].toArray();
    for (const auto &field : visibleFields) {
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

    // Render preview variants as tabs if present (per-group, per-locale, etc.)
    QJsonArray variants = data["variants"].toArray();
    if (!variants.isEmpty()) {
        auto *tabs = new QTabWidget;
        for (const auto &variant : variants) {
            QJsonObject v = variant.toObject();
            auto *page = new QWidget;
            auto *pageLayout = new QVBoxLayout(page);
            QJsonArray vFields = v["visible_fields"].toArray();
            for (const auto &vf : vFields) {
                QJsonObject f = vf.toObject();
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
            tabs->addTab(page, v["display_name"].toString());
        }
        // Select the active variant if specified
        QString selectedVariant = data["selected_variant"].toString();
        if (!selectedVariant.isEmpty()) {
            for (int i = 0; i < variants.size(); ++i) {
                if (variants[i].toObject()["variant_id"].toString() == selectedVariant) {
                    tabs->setCurrentIndex(i);
                    break;
                }
            }
        }
        // Wire tab switch to emit GroupViewSelected (frontend → core wire
        // surface; renamed to a UI-shaped action is a separate sweep
        // post-Wire-Humble Tier 1)
        if (onAction) {
            QObject::connect(tabs, &QTabWidget::currentChanged, tabs,
                             [onAction, variants](int index) {
                                 if (index < 0 || index >= variants.size()) return;
                                 QString variantId = variants[index].toObject()["variant_id"].toString();
                                 QJsonObject action;
                                 QJsonObject inner;
                                 inner["group_name"] = variantId;
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
