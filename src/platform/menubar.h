// SPDX-FileCopyrightText: 2026 Mattia Egloff <mattia.egloff@pm.me>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QMenuBar>

/// Application menu bar for Vauchi.
class VauchiMenuBar : public QMenuBar {
    Q_OBJECT

public:
    explicit VauchiMenuBar(QWidget *parent = nullptr);

private:
    // TODO: Add File, Edit, Help menus
};
