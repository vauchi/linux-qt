// SPDX-FileCopyrightText: 2026 Mattia Egloff <mattia.egloff@pm.me>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "coreui/presentationaccessibility.h"

#include <QAccessible>
#include <QAccessibleWidget>

PresentationDivider::PresentationDivider(QWidget *parent) : QFrame(parent) {
    setFrameShape(QFrame::HLine);
}

namespace {

QAccessibleInterface *presentationFactory(const QString &classname,
                                          QObject *object) {
    if (classname == QLatin1String("PresentationDivider") && object != nullptr
        && object->isWidgetType()) {
        return new QAccessibleWidget(static_cast<QWidget *>(object),
                                     QAccessible::Separator);
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
