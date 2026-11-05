// SPDX-FileCopyrightText: 2026 Mattia Egloff <mattia.egloff@pm.me>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QString>
class QAction;
class QMenu;

void installPreparedMenuActionAccessibility(QAction *action, QMenu *menu,
                                            const QString &label);
