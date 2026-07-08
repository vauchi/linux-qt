// SPDX-FileCopyrightText: 2026 Mattia Egloff <mattia.egloff@pm.me>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "dividercomponent.h"
#include "../../i18n.h"
#include <QFrame>

QWidget *DividerComponent::render() {
    auto *line = new QFrame;
    line->setFrameShape(QFrame::HLine);
    line->setFrameShadow(QFrame::Sunken);
    line->setAccessibleName(tr_vauchi("a11y.divider", "Separator"));
    return line;
}
