// SPDX-FileCopyrightText: 2026 Mattia Egloff <mattia.egloff@pm.me>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "presentationaccessibility.h"

#include <QAbstractItemView>
#include <QAccessible>
#include <QAccessibleWidget>
#include <QLineEdit>

PresentationDivider::PresentationDivider(QWidget *parent) : QFrame(parent) {
    setFrameShape(QFrame::HLine);
}

PresentationChoice::PresentationChoice(QWidget *parent) : QComboBox(parent) {}

namespace {

const QString kShowMenu = QStringLiteral("ShowMenu");

/// Combo box interface that answers with Core's label.
///
/// Replacing Qt's own interface is the only way to beat
/// `QAccessibleComboBox::text(Name)`, so the popup exposure and the
/// open-the-list action it provides are reimplemented here rather than lost.
class ChoiceAccessible : public QAccessibleWidget {
public:
    explicit ChoiceAccessible(PresentationChoice *choice)
        : QAccessibleWidget(choice, QAccessible::ComboBox) {}

    QString text(QAccessible::Text t) const override {
        switch (t) {
        case QAccessible::Name:
            return widget()->accessibleName().isEmpty()
                       ? QAccessibleWidget::text(t)
                       : widget()->accessibleName();
        case QAccessible::Value:
            return choice()->currentText();
        default:
            return QAccessibleWidget::text(t);
        }
    }

    int childCount() const override { return choice()->view() != nullptr ? 1 : 0; }

    QAccessibleInterface *child(int index) const override {
        if (index != 0 || choice()->view() == nullptr) {
            return nullptr;
        }
        return QAccessible::queryAccessibleInterface(choice()->view());
    }

    int indexOfChild(const QAccessibleInterface *child) const override {
        return child != nullptr && child->object() == choice()->view() ? 0 : -1;
    }

    QStringList actionNames() const override {
        return QStringList{kShowMenu} + QAccessibleWidget::actionNames();
    }

    QString localizedActionDescription(const QString &actionName) const override {
        return actionName == kShowMenu
                   ? QComboBox::tr("Open the combo box selection popup")
                   : QAccessibleWidget::localizedActionDescription(actionName);
    }

    void doAction(const QString &actionName) override {
        if (actionName == kShowMenu) {
            choice()->showPopup();
            return;
        }
        QAccessibleWidget::doAction(actionName);
    }

private:
    PresentationChoice *choice() const {
        return static_cast<PresentationChoice *>(widget());
    }
};

QAccessibleInterface *presentationFactory(const QString &classname,
                                          QObject *object) {
    if (object == nullptr || !object->isWidgetType()) {
        return nullptr;
    }
    if (classname == QLatin1String("PresentationDivider")) {
        return new QAccessibleWidget(static_cast<QWidget *>(object),
                                     QAccessible::Separator);
    }
    if (classname == QLatin1String("PresentationChoice")) {
        return new ChoiceAccessible(static_cast<PresentationChoice *>(object));
    }
    return nullptr;
}

} // namespace

void installPresentationAccessibilityFactory() {
    static bool installed = false;
    if (installed) {
        return;
    }
    QAccessible::installFactory(presentationFactory);
    installed = true;
}
