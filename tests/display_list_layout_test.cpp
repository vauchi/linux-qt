// SPDX-FileCopyrightText: 2026 Mattia Egloff <mattia.egloff@pm.me>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "coreui/presentationsurface.h"
#include "coreui/thememanager.h"
#include <QApplication>
#include <QAccessible>
#include <QCheckBox>
#include <QJsonArray>
#include <QScrollArea>
#include <QScrollBar>
#include <QStyleOptionButton>
#include <QToolButton>
#include <cassert>

namespace {

QJsonObject preparedSurface(bool enabled = true) {
    const auto action = QJsonObject{{"interaction_id", "activate-row"},
        {"label", "Contact update alerts"}, {"accessibility_label", "Contact update alerts"},
        {"enabled", true}};
    const auto toggle = QJsonObject{{"Toggle", QJsonObject{{"binding_id", "updates"},
        {"label", ""}, {"value", false}, {"enabled", enabled},
        {"accessibility", QJsonObject{{"label", "Contact update alerts"}}}}}};
    const auto row = QJsonObject{{"title", "Contact update alerts"},
        {"subtitle", "Show a notice when a friend edits a shared card. You can change this at any time. "
                     "This longer line must wrap when the window is narrow or the text is large."},
        {"detail", "Extra row detail"}, {"activation", action}, {"enabled", true}, {"controls", QJsonArray{toggle}},
        {"accessibility", QJsonObject{{"label", "Contact update alerts"}}}};
    return {{"surface_id", "layout-test"}, {"revision", 1}, {"title", "Preferences"},
        {"layout", "scroll"}, {"tokens", QJsonObject{{"minimum_target_size", 48}}},
        {"nodes", QJsonArray{QJsonObject{{"List", QJsonObject{
            {"label", "Alerts"}, {"rows", QJsonArray{row}}}}}}}};
}

void settle(PresentationSurface &surface, int width) {
    surface.resize(width, 600);
    surface.show();
    QCoreApplication::sendPostedEvents(nullptr, QEvent::LayoutRequest);
    QApplication::processEvents();
}

// @scenario: generic_presentation_protocol.feature :: Every shell renders the same prepared presentation
void inlineControl() {
    PresentationSurface surface(preparedSurface());
    settle(surface, 600);
    auto *action = surface.findChild<QToolButton *>("activate-row");
    auto *control = surface.findChild<QCheckBox *>("updates");
    assert(action && control);
    const auto top = action->mapToGlobal(QPoint{}).y();
    const auto center = control->mapToGlobal(control->rect().center()).y();
    assert(center >= top && center < top + action->height());
}

// @scenario: generic_presentation_protocol.feature :: Available window drives structural composition
void reflow() {
    PresentationSurface surface(preparedSurface());
    settle(surface, 360);
    auto *scroll = surface.findChild<QScrollArea *>();
    assert(scroll);
    assert(surface.width() == 360);
    assert(scroll->horizontalScrollBar()->maximum() == 0);
}

// @scenario: generic_presentation_protocol.feature :: User interaction returns as an opaque event
void events() {
    PresentationSurface surface(preparedSurface());
    QString surfaceId;
    QString interaction;
    QString binding;
    QJsonValue value;
    QObject::connect(&surface, &PresentationSurface::interactionReady,
        [&](const QString &id, const QString &action) { surfaceId = id; interaction = action; });
    QObject::connect(&surface, &PresentationSurface::valueReady,
        [&](const QString &id, const QString &key, const QJsonValue &raw) {
            surfaceId = id; binding = key; value = raw;
        });
    surface.findChild<QToolButton *>("activate-row")->click();
    assert(surfaceId == "layout-test" && interaction == "activate-row");
    surface.findChild<QCheckBox *>("updates")->click();
    assert(surfaceId == "layout-test" && binding == "updates");
    assert(value == QJsonValue(QJsonObject{{"boolean", true}}));
}

// @internal — rearranging a row must preserve Core's disabled control state.
void disabledControl() {
    PresentationSurface surface(preparedSurface(false));
    auto *control = surface.findChild<QCheckBox *>("updates");
    assert(control && !control->isEnabled());
    control->click();
    assert(!control->isChecked());
}

void visibleCheckbox() {
    for (const bool light : {false, true}) {
        if (light) ThemeManager::applyDefaultLightTheme();
        else ThemeManager::applyDefaultTheme();
        PresentationSurface surface(preparedSurface());
        settle(surface, 600);
        auto *control = surface.findChild<QCheckBox *>("updates");
        QStyleOptionButton option;
        option.initFrom(control);
        const auto indicator = control->style()->subElementRect(
            QStyle::SE_CheckBoxIndicator, &option, control);
        assert(indicator.width() >= 22 && indicator.height() >= 22);
        const auto unchecked = control->grab().toImage();
        assert(unchecked.pixelColor(indicator.left() + 1, indicator.center().y())
            == control->palette().color(QPalette::Text));
        control->click();
        const auto checked = control->grab().toImage();
        int checkPixels = 0;
        for (int y = indicator.top(); y <= indicator.bottom(); ++y)
            for (int x = indicator.left(); x <= indicator.right(); ++x)
                if (checked.pixelColor(x, y) == control->palette().color(QPalette::HighlightedText))
                    ++checkPixels;
        assert(checkPixels >= 8);
        assert(control->isChecked());
    }
}

bool accessibleText(QAccessibleInterface *node, const QString &text) {
    if (node->text(QAccessible::Name) == text) return true;
    for (int index = 0; index < node->childCount(); ++index) {
        auto *child = node->child(index);
        if (child && accessibleText(child, text)) return true;
    }
    return false;
}

// @internal — native buttons hide their descendants from the a11y tree (#1109).
void rowDetail() {
    PresentationSurface surface(preparedSurface());
    settle(surface, 600);
    auto *accessible = QAccessible::queryAccessibleInterface(&surface);
    assert(accessible && accessibleText(accessible, "Extra row detail"));
}
}

int main(int argc, char **argv) {
    QApplication app(argc, argv);
    assert(argc == 2);
    const QString test = QString::fromLocal8Bit(argv[1]);
    if (test == "inline") inlineControl();
    else if (test == "reflow") reflow();
    else if (test == "large_text") {
        auto font = app.font();
        font.setPointSizeF(font.pointSizeF() * 2);
        app.setFont(font);
        reflow();
    } else if (test == "events") events();
    else if (test == "disabled") disabledControl();
    else if (test == "checkbox") visibleCheckbox();
    else if (test == "detail") rowDetail();
    else return 2;
}
