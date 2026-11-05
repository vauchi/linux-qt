// SPDX-FileCopyrightText: 2026 Mattia Egloff <mattia.egloff@pm.me>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "applicationmenuaccessibility.h"
#include <QAccessibleObject>
#include <QAction>
#include <QMenu>
#include <QPointer>

namespace {

// Qt's menu child lookup uses the action's registered interface; its default
// names actions from visible text rather than Core's accessibility label (#1073).
class PreparedMenuActionAccessible : public QAccessibleObject,
                                     public QAccessibleActionInterface {
public:
    PreparedMenuActionAccessible(QAction *action, QMenu *menu, const QString &label)
        : QAccessibleObject(action), m_menu(menu), m_label(label) {}

    QAccessibleInterface *parent() const override {
        return QAccessible::queryAccessibleInterface(m_menu);
    }
    QAccessibleInterface *child(int) const override { return nullptr; }
    int childCount() const override { return 0; }
    int indexOfChild(const QAccessibleInterface *) const override { return -1; }
    QAccessible::Role role() const override { return QAccessible::MenuItem; }
    QRect rect() const override {
        if (!m_menu || !action()) return {};
        return m_menu->actionGeometry(action()).translated(m_menu->mapToGlobal(QPoint()));
    }
    QString text(QAccessible::Text type) const override {
        if (type == QAccessible::Name) return m_label;
        if (type == QAccessible::Accelerator && action()) return action()->shortcut().toString();
        return {};
    }
    QAccessible::State state() const override {
        QAccessible::State result;
        result.disabled = !action() || !action()->isEnabled();
        result.invisible = !m_menu || !m_menu->isVisible() || !action() || !action()->isVisible();
        result.focusable = !result.disabled;
        result.focused = m_menu && m_menu->activeAction() == action();
        return result;
    }
    void *interface_cast(QAccessible::InterfaceType type) override {
        return type == QAccessible::ActionInterface
            ? static_cast<QAccessibleActionInterface *>(this) : nullptr;
    }
    QStringList actionNames() const override { return {pressAction()}; }
    void doAction(const QString &name) override {
        if (name == pressAction() && action() && action()->isEnabled()) action()->trigger();
    }
    QStringList keyBindingsForAction(const QString &name) const override {
        if (name == pressAction() && action() && !action()->shortcut().isEmpty())
            return {action()->shortcut().toString()};
        return {};
    }

private:
    QAction *action() const { return qobject_cast<QAction *>(object()); }
    QPointer<QMenu> m_menu;
    QString m_label;
};

} // namespace

void installPreparedMenuActionAccessibility(QAction *action, QMenu *menu,
                                            const QString &label) {
    QAccessible::registerAccessibleInterface(new PreparedMenuActionAccessible(action, menu, label));
}
