// SPDX-FileCopyrightText: 2026 Mattia Egloff <mattia.egloff@pm.me>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "presentationsurface.h"

#include "presentationaccessibility.h"

#include <QCheckBox>
#include <QComboBox>
#include <QEvent>
#include <QFrame>
#include <QGroupBox>
#include <QJsonArray>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QIcon>
#include <QPixmap>
#include <QProgressBar>
#include <QPushButton>
#include <QScrollArea>
#include <QSlider>
#include <QToolButton>
#include <QWidgetAction>
#include <QVBoxLayout>

PresentationSurface::PresentationSurface(const QJsonObject &surface,
                                         QWidget *parent)
    : QWidget(parent),
      m_surfaceId(surface.value(QStringLiteral("surface_id")).toString()) {
    const QJsonObject tokens = surface.value(QStringLiteral("tokens")).toObject();
    m_minimumTargetSize = tokens.value(QStringLiteral("minimum_target_size"))
                              .toInt(m_minimumTargetSize);
    m_cornerRadius =
        tokens.value(QStringLiteral("corner_radius")).toInt(m_cornerRadius);
    installPresentationAccessibilityFactory();
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
            return new PresentationDivider;
        }
        return new QLabel;
    }
    const QJsonObject node = nodeValue.toObject();
    if (node.contains(QStringLiteral("Text"))) {
        const QJsonObject payload = node.value(QStringLiteral("Text")).toObject();
        auto *label =
            new QLabel(payload.value(QStringLiteral("content")).toString());
        label->setWordWrap(true);
        const QString textStyle = payload.value(QStringLiteral("style")).toString();
        if (textStyle == QStringLiteral("heading")) {
            // The brand heading family (Bricolage Grotesque, weight 700)
            // comes from ThemeManager's generated stylesheet, keyed off
            // this property — see stylesheetFromColors().
            label->setProperty("textStyle", textStyle);
            QFont font = label->font();
            font.setBold(true);
            label->setFont(font);
        } else if (textStyle == QStringLiteral("monospace")) {
            label->setProperty("textStyle", textStyle);
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
        connect(input, &QLineEdit::returnPressed, this,
                [this, binding]() { emit submitReady(m_surfaceId, binding); });
        // QLineEdit has no focus-out signal, and `editingFinished` fires
        // for Return as well — which would report both gestures for one
        // press. Watching the event directly keeps them distinct.
        input->installEventFilter(this);
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
        toggle->setMinimumHeight(m_minimumTargetSize);
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
        auto *choice = new PresentationChoice;
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
        button->setMinimumHeight(m_minimumTargetSize);
        button->setStyleSheet(targetSizeStyleSheet());
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
                // QAction exposes no accessible-name API, so its Core label
                // would be dropped; a widget action carries one.
                auto *item = new QToolButton(menu);
                item->setText(action.value(QStringLiteral("label")).toString());
                item->setToolButtonStyle(Qt::ToolButtonTextOnly);
                item->setAutoRaise(true);
                item->setSizePolicy(QSizePolicy::Expanding,
                                    QSizePolicy::Preferred);
                item->setEnabled(
                    action.value(QStringLiteral("enabled")).toBool(true));
                item->setAccessibleName(
                    action.value(QStringLiteral("accessibility_label"))
                        .toString());
                auto *menuAction = new QWidgetAction(menu);
                // A widget action paints its default widget, so the action's
                // own text is free to be the accessible name Qt reads for the
                // menu item.
                menuAction->setText(
                    action.value(QStringLiteral("accessibility_label"))
                        .toString());
                menuAction->setDefaultWidget(item);
                menu->addAction(menuAction);
                connect(item, &QToolButton::clicked, this,
                        [this, action, menu]() {
                            menu->hide();
                            activate(action);
                        });
            }
            button->setMenu(menu);
            button->setPopupMode(QToolButton::MenuButtonPopup);
        }
        applyAccessibility(
            button, row.value(QStringLiteral("accessibility")).toObject());

        auto *rowWidget = new QWidget;
        auto *rowLayout = new QHBoxLayout(rowWidget);
        rowLayout->setContentsMargins(0, 0, 0, 0);
        const QJsonArray avatar =
            row.value(QStringLiteral("image_data")).toArray();
        const QString initials =
            row.value(QStringLiteral("fallback_text")).toString();
        if (!avatar.isEmpty()) {
            QByteArray bytes;
            bytes.reserve(avatar.size());
            for (const auto &byte : avatar) {
                bytes.append(static_cast<char>(byte.toInt()));
            }
            QPixmap pixmap;
            if (pixmap.loadFromData(bytes)) {
                auto *image = new QLabel;
                image->setPixmap(pixmap.scaled(32, 32, Qt::KeepAspectRatio,
                                               Qt::SmoothTransformation));
                rowLayout->addWidget(image);
            }
        } else if (!initials.isEmpty()) {
            rowLayout->addWidget(new QLabel(initials));
        }
        rowLayout->addWidget(button, 1);
        const QString detail = row.value(QStringLiteral("detail")).toString();
        if (!detail.isEmpty()) {
            auto *detailLabel = new QLabel(detail);
            detailLabel->setProperty("tone", "muted");
            rowLayout->addWidget(detailLabel);
        }
        // A row without an activation has no button to carry its name, and
        // the avatar has to sit inside whatever does.
        if (activation.isEmpty()) {
            applyAccessibility(
                rowWidget, row.value(QStringLiteral("accessibility")).toObject());
        }
        layout->addWidget(rowWidget);
        for (const auto &control :
             row.value(QStringLiteral("controls")).toArray()) {
            layout->addWidget(renderNode(control));
        }
    }
    applyAccessibility(
        group, payload.value(QStringLiteral("accessibility")).toObject());
    return group;
}

QString PresentationSurface::targetSizeStyleSheet() const {
    return QStringLiteral("border-radius: %1px;").arg(m_cornerRadius);
}

bool PresentationSurface::eventFilter(QObject *watched, QEvent *event) {
    // The binding id is the widget's object name, set where the field is
    // built. Anything else watched here has none and is ignored.
    if (event->type() == QEvent::FocusOut) {
        const QString binding = watched->objectName();
        if (!binding.isEmpty()) {
            emit focusEndReady(m_surfaceId, binding);
        }
    }
    return QWidget::eventFilter(watched, event);
}
