// SPDX-FileCopyrightText: 2026 Mattia Egloff <mattia.egloff@pm.me>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QFrame>

/// A `PresentationNode::Divider` rendered as its own type so the accessibility
/// factory can give it the `Separator` role.
///
/// Qt derives an accessible role from the widget class and offers no
/// per-instance setter, so a bare QFrame reaches AT-SPI as a `panel` — a
/// container a screen reader will try to descend into rather than a rule it
/// can announce and skip. See
/// `problems/2026-08-21-linux-shells-drop-core-a11y`.
class PresentationDivider : public QFrame {
    Q_OBJECT

public:
    explicit PresentationDivider(QWidget *parent = nullptr);
};

/// Register the accessible interfaces for vauchi's own presentation widgets.
/// Idempotent; safe to call from every surface construction.
void installPresentationAccessibilityFactory();
