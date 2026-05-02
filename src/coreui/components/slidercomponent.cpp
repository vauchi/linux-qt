// SPDX-FileCopyrightText: 2026 Mattia Egloff <mattia.egloff@pm.me>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "slidercomponent.h"
#include <QHBoxLayout>
#include <QLabel>
#include <QSlider>
#include <QVBoxLayout>

QWidget *SliderComponent::render(const QJsonObject &data,
                                 const OnAction &onAction) {
    auto *container = new QWidget;
    auto *outer = new QVBoxLayout(container);
    QString componentId = data["id"].toString();
    container->setObjectName(componentId);

    QString label = data["label"].toString();
    if (!label.isEmpty()) {
        outer->addWidget(new QLabel(label));
    }

    auto *row = new QHBoxLayout;

    // Convert f32 [min, max] to int [min*1000, max*1000] for QSlider; the
    // SliderChanged action carries value_milli (value scaled by 1000).
    double min = data["min"].toDouble();
    double max = data["max"].toDouble();
    double value = data["value"].toDouble();
    double step = data["step"].toDouble();

    auto *slider = new QSlider(Qt::Horizontal);
    slider->setMinimum(static_cast<int>(min * 1000.0));
    slider->setMaximum(static_cast<int>(max * 1000.0));
    slider->setValue(static_cast<int>(value * 1000.0));
    if (step > 0.0) {
        slider->setSingleStep(static_cast<int>(step * 1000.0));
        slider->setPageStep(static_cast<int>(step * 1000.0));
    }
    slider->setAccessibleName(label);

    if (data.contains("a11y") && data["a11y"].isObject()) {
        auto a11y = data["a11y"].toObject();
        auto a11yLabel = a11y.value("label").toString();
        if (!a11yLabel.isEmpty()) slider->setAccessibleName(a11yLabel);
    }

    if (onAction) {
        QObject::connect(slider, &QSlider::valueChanged, slider,
                         [onAction, componentId](int valueMilli) {
                             QJsonObject action;
                             QJsonObject inner;
                             inner["component_id"] = componentId;
                             inner["value_milli"] = valueMilli;
                             action["SliderChanged"] = inner;
                             onAction(action);
                         });
    }

    row->addWidget(slider, 1);
    outer->addLayout(row);
    return container;
}
