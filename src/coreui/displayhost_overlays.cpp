// SPDX-FileCopyrightText: 2026 Mattia Egloff <mattia.egloff@pm.me>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "displayhost.h"
#include "navigationicons.h"
#include <QDialog>
#include <QJsonArray>
#include <QLabel>
#include <QMenu>
#include <QPushButton>
#include <QScrollArea>
#include <QStyle>
#include <QVBoxLayout>

void QtDisplayHost::closeOverlay() {
    if (m_overlay) {
        m_overlay->setProperty("contract_silent", true);
        m_overlay->close();
        m_overlay = nullptr;
    }
    m_overlaySurface.clear();
    m_overlayKind.clear();
}

void QtDisplayHost::overlayDismissed(QWidget *widget, const QString &surfaceId,
                                    const QString &kind) {
    if (!widget->property("contract_silent").toBool()) {
        emitEvent({{"OverlayDismissed", QJsonObject{{"surface_id", surfaceId},
            {"kind", kind}}}});
    }
    if (m_overlay == widget) {
        m_overlay = nullptr;
        m_overlaySurface.clear();
        m_overlayKind.clear();
    }
}

void QtDisplayHost::applyOverlay(const QJsonObject &command) {
    const bool dismiss = command.contains("DismissOverlay");
    const auto payload = command.value(dismiss ? "DismissOverlay" : "PresentOverlay").toObject();
    const auto id = payload.value("surface_id").toString();
    const auto surface = m_state.surface(id);
    if (!surface || payload.value("revision").toInteger(-1)
                    != surface->value("revision").toInteger()) return;
    const auto overlay = payload.value("overlay").toObject();
    const auto kind = (dismiss ? payload.value("kind") : overlay.value("kind")).toString();
    if (dismiss) {
        if (id == m_overlaySurface && kind == m_overlayKind) closeOverlay();
        return;
    }
    closeOverlay();
    m_overlaySurface = id;
    m_overlayKind = kind;
    const auto title = overlay.value("title").toString();
    const auto items = overlay.value("items").toArray();
    const int target = surface->value("tokens").toObject().value("minimum_target_size").toInt(48);
    if (kind == "information" || kind == "navigation") {
        auto *dialog = new QDialog(this);
        dialog->setAttribute(Qt::WA_DeleteOnClose);
        dialog->setWindowTitle(title);
        m_overlay = dialog;
        auto *outer = new QVBoxLayout(dialog);
        auto *scroller = new QScrollArea(dialog);
        scroller->setWidgetResizable(true);
        auto *content = new QWidget(scroller);
        auto *layout = new QVBoxLayout(content);
        scroller->setWidget(content);
        outer->addWidget(scroller);
        if (kind == "information") {
            auto *body = new QLabel(overlay.value("body").toString());
            body->setWordWrap(true);
            body->setTextInteractionFlags(Qt::TextSelectableByMouse);
            layout->addWidget(body);
            auto *close = new QPushButton(overlay.value("close_label").toString());
            if (close->text().isEmpty()) {
                close->setIcon(style()->standardIcon(QStyle::SP_DialogCloseButton));
                close->setAccessibleName(title);
            }
            close->setMinimumHeight(target);
            close->setDefault(true);
            layout->addWidget(close);
            connect(close, &QPushButton::clicked, dialog, &QDialog::accept);
        } else {
            for (const auto &value : items) {
                const auto item = value.toObject();
                auto *button = new QPushButton(item.value("label").toString());
                button->setAccessibleName(item.value("accessibility_label").toString());
                button->setIcon(vauchi::navigationIcon(item.value("icon_token").toString()));
                button->setEnabled(item.value("enabled").toBool(true));
                button->setMinimumHeight(target);
                layout->addWidget(button);
                connect(button, &QPushButton::clicked, this, [this, dialog, id, item] {
                    dialog->setProperty("contract_silent", true);
                    dialog->accept();
                    activate(id, item.value("interaction_id").toString());
                });
            }
        }
        connect(dialog, &QDialog::finished, this, [this, dialog, id, kind](int) {
            overlayDismissed(dialog, id, kind);
        });
        dialog->show();
        return;
    }
    auto *menu = new QMenu(this);
    menu->setTitle(title);
    m_overlay = menu;
    for (const auto &value : items) {
        const auto item = value.toObject();
        auto *action = menu->addAction(item.value("label").toString());
        action->setEnabled(item.value("enabled").toBool(true));
        const auto icon = item.value("icon_token").toString();
        if (!icon.isEmpty()) action->setIcon(vauchi::navigationIcon(icon));
        connect(action, &QAction::triggered, this, [this, menu, id, item] {
            menu->setProperty("contract_silent", true);
            activate(id, item.value("interaction_id").toString());
        });
    }
    connect(menu, &QMenu::aboutToHide, this, [this, menu, id, kind] {
        overlayDismissed(menu, id, kind);
        menu->deleteLater();
    }, Qt::QueuedConnection); // Qt hides before triggering the chosen action (#1073).
    menu->popup(mapToGlobal(rect().center()));
}
