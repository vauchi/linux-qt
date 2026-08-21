// SPDX-FileCopyrightText: 2026 Mattia Egloff <mattia.egloff@pm.me>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QComboBox>
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

/// A `PresentationNode::Choice` rendered as its own type so the accessibility
/// factory can restore Core's label as the accessible name.
///
/// `QAccessibleComboBox::text(Name)` returns the current item instead of the
/// widget's `accessibleName` on Unix, so Core's label never reaches the bus
/// while the *description* does. The class lives in a private header and is
/// not exported, so it cannot be subclassed — the factory has to supply a
/// replacement interface built on public API.
class PresentationChoice : public QComboBox {
    Q_OBJECT

public:
    explicit PresentationChoice(QWidget *parent = nullptr);
};

/// Register the accessible interfaces for vauchi's own presentation widgets.
/// Idempotent; safe to call from every surface construction.
void installPresentationAccessibilityFactory();
