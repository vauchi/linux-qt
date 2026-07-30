// SPDX-FileCopyrightText: 2026 Mattia Egloff <mattia.egloff@pm.me>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "presentationsurface.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFrame>
#include <QGroupBox>
#include <QJsonArray>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QProgressBar>
#include <QPushButton>
#include <QScrollArea>
#include <QSlider>
#include <QToolButton>
#include <QVBoxLayout>

PresentationSurface::PresentationSurface(const QJsonObject &surface,
                                         QWidget *parent)
    : QWidget(parent),
      m_surfaceId(surface.value(QStringLiteral("surface_id")).toString()) {
    setAccessibleName(
        surface.value(QStringLiteral("accessibility_label")).toString());
    auto *outer = new QVBoxLayout(this);
    auto *title = new QLabel(surface.value(QStringLiteral("title")).toString());
    title->setObjectName(QStringLiteral("screen_title"));
    QFont titleFont = title->font();
    titleFont.setPointSize(titleFont.pointSize() + 5);
    titleFont.setBold(true);
    title->setFont(titleFont);
    outer->addWidget(title);
    const QString subtitle =
        surface.value(QStringLiteral("subtitle")).toString();
    if (!subtitle.isEmpty()) {
        auto *label = new QLabel(subtitle);
        label->setWordWrap(true);
        outer->addWidget(label);
    }

    auto *content = new QWidget;
    auto *contentLayout = new QVBoxLayout(content);
    for (const auto &value : surface.value(QStringLiteral("nodes")).toArray()) {
        contentLayout->addWidget(renderNode(value));
    }
    contentLayout->addStretch();
    if (surface.value(QStringLiteral("layout")).toString()
        == QStringLiteral("fixed")) {
        outer->addWidget(content, 1);
    } else {
        auto *scroll = new QScrollArea;
        scroll->setWidgetResizable(true);
        scroll->setFrameShape(QFrame::NoFrame);
        scroll->setWidget(content);
        outer->addWidget(scroll, 1);
    }
}

QWidget *PresentationSurface::renderNode(const QJsonValue &nodeValue) {
    if (nodeValue.isString()) {
        if (nodeValue.toString() == QStringLiteral("Divider")) {
            auto *divider = new QFrame;
            divider->setFrameShape(QFrame::HLine);
            return divider;
        }
        return new QLabel;
    }
    const QJsonObject node = nodeValue.toObject();
    if (node.contains(QStringLiteral("Text"))) {
        const QJsonObject payload = node.value(QStringLiteral("Text")).toObject();
        auto *label =
            new QLabel(payload.value(QStringLiteral("content")).toString());
        label->setWordWrap(true);
        if (payload.value(QStringLiteral("style")).toString()
            == QStringLiteral("heading")) {
            QFont font = label->font();
            font.setBold(true);
            label->setFont(font);
        }
        applyAccessibility(
            label, payload.value(QStringLiteral("accessibility")).toObject());
        return label;
    }
    if (node.contains(QStringLiteral("Input"))) {
        const QJsonObject payload =
            node.value(QStringLiteral("Input")).toObject();
        auto *container = new QGroupBox(
            payload.value(QStringLiteral("label")).toString());
        auto *layout = new QVBoxLayout(container);
        auto *input = new QLineEdit(
            payload.value(QStringLiteral("value")).toString());
        input->setPlaceholderText(
            payload.value(QStringLiteral("placeholder")).toString());
        input->setEnabled(payload.value(QStringLiteral("enabled")).toBool(true));
        input->setEchoMode(
            payload.value(QStringLiteral("input_kind")).toString()
                    == QStringLiteral("password")
                ? QLineEdit::Password
                : QLineEdit::Normal);
        const int maxLength =
            payload.value(QStringLiteral("max_length")).toInt();
        if (maxLength > 0) {
            input->setMaxLength(maxLength);
        }
        applyAccessibility(
            input, payload.value(QStringLiteral("accessibility")).toObject());
        const QString binding =
            payload.value(QStringLiteral("binding_id")).toString();
        input->setObjectName(binding);
        connect(input, &QLineEdit::textChanged, this,
                [this, binding](const QString &text) {
                    emit valueReady(
                        m_surfaceId, binding,
                        QJsonObject{{QStringLiteral("text"), text}});
                });
        layout->addWidget(input);
        const QString error =
            payload.value(QStringLiteral("validation_error")).toString();
        if (!error.isEmpty()) {
            auto *errorLabel = new QLabel(error);
            errorLabel->setProperty("tone", "error");
            layout->addWidget(errorLabel);
        }
        return container;
    }
    if (node.contains(QStringLiteral("Toggle"))) {
        const QJsonObject payload =
            node.value(QStringLiteral("Toggle")).toObject();
        auto *toggle = new QCheckBox(
            payload.value(QStringLiteral("label")).toString());
        toggle->setChecked(payload.value(QStringLiteral("value")).toBool());
        toggle->setEnabled(payload.value(QStringLiteral("enabled")).toBool(true));
        applyAccessibility(
            toggle, payload.value(QStringLiteral("accessibility")).toObject());
        const QString binding =
            payload.value(QStringLiteral("binding_id")).toString();
        toggle->setObjectName(binding);
        connect(toggle, &QCheckBox::toggled, this,
                [this, binding](bool checked) {
                    emit valueReady(
                        m_surfaceId, binding,
                        QJsonObject{{QStringLiteral("boolean"), checked}});
                });
        return toggle;
    }
    if (node.contains(QStringLiteral("Choice"))) {
        const QJsonObject payload =
            node.value(QStringLiteral("Choice")).toObject();
        auto *container = new QGroupBox(
            payload.value(QStringLiteral("label")).toString());
        auto *layout = new QVBoxLayout(container);
        auto *choice = new QComboBox;
        const QString selected =
            payload.value(QStringLiteral("selected")).toString();
        for (const auto &optionValue :
             payload.value(QStringLiteral("options")).toArray()) {
            const QJsonObject option = optionValue.toObject();
            choice->addItem(option.value(QStringLiteral("label")).toString(),
                            option.value(QStringLiteral("id")).toString());
        }
        choice->setCurrentIndex(choice->findData(selected));
        choice->setEnabled(payload.value(QStringLiteral("enabled")).toBool(true));
        applyAccessibility(
            choice, payload.value(QStringLiteral("accessibility")).toObject());
        const QString binding =
            payload.value(QStringLiteral("binding_id")).toString();
        choice->setObjectName(binding);
        connect(choice, &QComboBox::currentIndexChanged, this,
                [this, choice, binding](int) {
                    emit valueReady(
                        m_surfaceId, binding,
                        QJsonObject{
                            {QStringLiteral("choice"),
                             choice->currentData().toString()}});
                });
        layout->addWidget(choice);
        return container;
    }
    if (node.contains(QStringLiteral("Group"))) {
        return renderGroup(node.value(QStringLiteral("Group")).toObject());
    }
    if (node.contains(QStringLiteral("List"))) {
        return renderList(node.value(QStringLiteral("List")).toObject());
    }
    if (node.contains(QStringLiteral("Status"))) {
        return renderStatus(node.value(QStringLiteral("Status")).toObject());
    }
    if (node.contains(QStringLiteral("Confirmation"))) {
        return renderConfirmation(
            node.value(QStringLiteral("Confirmation")).toObject());
    }
    if (node.contains(QStringLiteral("Image"))) {
        return renderImage(node.value(QStringLiteral("Image")).toObject());
    }
    if (node.contains(QStringLiteral("Qr"))) {
        return renderQr(node.value(QStringLiteral("Qr")).toObject());
    }
    if (node.contains(QStringLiteral("Slider"))) {
        const QJsonObject payload =
            node.value(QStringLiteral("Slider")).toObject();
        auto *container = new QGroupBox(
            payload.value(QStringLiteral("label")).toString());
        auto *layout = new QVBoxLayout(container);
        auto *slider = new QSlider(Qt::Horizontal);
        constexpr int scale = 1000;
        const double minimum =
            payload.value(QStringLiteral("minimum")).toDouble();
        const double maximum =
            payload.value(QStringLiteral("maximum")).toDouble();
        slider->setRange(0, scale);
        slider->setValue(static_cast<int>(
            scale * (payload.value(QStringLiteral("value")).toDouble()
                     - minimum)
            / qMax(0.0001, maximum - minimum)));
        const QString binding =
            payload.value(QStringLiteral("binding_id")).toString();
        slider->setObjectName(binding);
        applyAccessibility(
            slider, payload.value(QStringLiteral("accessibility")).toObject());
        connect(slider, &QSlider::valueChanged, this,
                [this, binding, minimum, maximum](int value) {
                    const double number =
                        minimum + (maximum - minimum) * value / 1000.0;
                    emit valueReady(
                        m_surfaceId, binding,
                        QJsonObject{{QStringLiteral("number"), number}});
                });
        layout->addWidget(slider);
        return container;
    }
    if (node.contains(QStringLiteral("Progress"))) {
        const QJsonObject payload =
            node.value(QStringLiteral("Progress")).toObject();
        auto *progress = new QProgressBar;
        progress->setFormat(
            payload.value(QStringLiteral("label")).toString());
        if (payload.value(QStringLiteral("value")).isNull()) {
            progress->setRange(0, 0);
        } else {
            progress->setRange(0, 1000);
            progress->setValue(static_cast<int>(
                payload.value(QStringLiteral("value")).toDouble() * 1000));
        }
        applyAccessibility(
            progress, payload.value(QStringLiteral("accessibility")).toObject());
        return progress;
    }
    return new QLabel;
}

QWidget *PresentationSurface::renderGroup(const QJsonObject &payload) {
    auto *group =
        new QGroupBox(payload.value(QStringLiteral("label")).toString());
    QBoxLayout *layout =
        payload.value(QStringLiteral("axis")).toString()
                == QStringLiteral("horizontal")
            ? static_cast<QBoxLayout *>(new QHBoxLayout(group))
            : static_cast<QBoxLayout *>(new QVBoxLayout(group));
    for (const auto &child :
         payload.value(QStringLiteral("children")).toArray()) {
        layout->addWidget(renderNode(child));
    }
    applyAccessibility(
        group, payload.value(QStringLiteral("accessibility")).toObject());
    return group;
}

QWidget *PresentationSurface::renderList(const QJsonObject &payload) {
    auto *group =
        new QGroupBox(payload.value(QStringLiteral("label")).toString());
    auto *layout = new QVBoxLayout(group);
    for (const auto &rowValue :
         payload.value(QStringLiteral("rows")).toArray()) {
        const QJsonObject row = rowValue.toObject();
        auto *button = new QToolButton;
        QString text = row.value(QStringLiteral("title")).toString();
        const QString subtitle =
            row.value(QStringLiteral("subtitle")).toString();
        if (!subtitle.isEmpty()) {
            text += QStringLiteral("\n") + subtitle;
        }
        button->setText(text);
        button->setToolButtonStyle(Qt::ToolButtonTextOnly);
        button->setEnabled(row.value(QStringLiteral("enabled")).toBool(true));
        button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        const QJsonObject activation =
            row.value(QStringLiteral("activation")).toObject();
        if (!activation.isEmpty()) {
            button->setObjectName(
                activation.value(QStringLiteral("interaction_id")).toString());
            connect(button, &QToolButton::clicked, this,
                    [this, activation]() { activate(activation); });
        }
        const QJsonArray secondary =
            row.value(QStringLiteral("secondary_actions")).toArray();
        if (!secondary.isEmpty()) {
            auto *menu = new QMenu(button);
            for (const auto &actionValue : secondary) {
                const QJsonObject action = actionValue.toObject();
                auto *menuAction = menu->addAction(
                    action.value(QStringLiteral("label")).toString());
                menuAction->setEnabled(
                    action.value(QStringLiteral("enabled")).toBool(true));
                connect(menuAction, &QAction::triggered, this,
                        [this, action]() { activate(action); });
            }
            button->setMenu(menu);
            button->setPopupMode(QToolButton::MenuButtonPopup);
        }
        applyAccessibility(
            button, row.value(QStringLiteral("accessibility")).toObject());
        layout->addWidget(button);
        for (const auto &control :
             row.value(QStringLiteral("controls")).toArray()) {
            layout->addWidget(renderNode(control));
        }
    }
    return group;
}
