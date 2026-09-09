// SPDX-FileCopyrightText: 2026 Mattia Egloff <mattia.egloff@pm.me>
// SPDX-License-Identifier: GPL-3.0-or-later

/// The File menu's import entry renders Core-provided menu copy; without Core
/// the shell falls back to a neutral label with no domain vocabulary (ADR-066).

#include "../src/i18n.h"
#include "../src/platform/menubar.h"
#include <QAction>
#include <QApplication>
#include <QKeySequence>
#include <QMenu>
#include <cassert>
#include <cstdio>

static QAction *importAction(VauchiMenuBar &menuBar) {
    for (QAction *top : menuBar.actions()) {
        QMenu *menu = top->menu();
        if (!menu) continue;
        for (QAction *action : menu->actions()) {
            if (action->shortcut() == QKeySequence(Qt::CTRL | Qt::Key_I)) {
                return action;
            }
        }
    }
    return nullptr;
}

// Without a Core i18n symbol every tr_vauchi label is its shell fallback;
// with one, Core copy comes back even before vauchi_i18n_init. Both paths
// must route the entry through Core's `menu.*` key, never a domain key.
static void test_import_entry_is_core_menu_copy_or_neutral() {
    VauchiMenuBar menuBar;
    QAction *action = importAction(menuBar);
    assert(action != nullptr);
    assert(action->text()
           == tr_vauchi("menu.import_contacts", QStringLiteral("Import\u2026")));
    assert(action->text()
           != tr_vauchi("contacts.importContacts", QStringLiteral("Import Contacts")));
    const bool coreCopyReachable = !tr_vauchi("menu.file", QString()).isEmpty();
    if (!coreCopyReachable) {
        assert(action->text() == QStringLiteral("Import\u2026"));
    }
    printf("  PASS: import_entry_is_core_menu_copy_or_neutral (core copy: %s)\n",
           coreCopyReachable ? "yes" : "no");
}

int main(int argc, char **argv) {
    QApplication app(argc, argv);
    printf("menubar_test\n");
    test_import_entry_is_core_menu_copy_or_neutral();
    printf("ALL TESTS PASSED\n");
    return 0;
}
