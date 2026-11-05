// SPDX-FileCopyrightText: 2026 Mattia Egloff <mattia.egloff@pm.me>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "display_test_harness.h"
#include <QColor>
#include <QEventLoop>
#include <QLabel>
#include <QLineEdit>
#include <QPalette>
#include <QPointer>
#include <QPushButton>

using namespace display_test;
namespace {
QPointer<QLineEdit> keptInput;

QJsonObject colors(bool optional = true) {
    QJsonObject result{{"background", "#102030"}, {"surface", "#203040"},
        {"text", "#e0e1e2"}, {"text_muted", "#a0a1a2"}, {"accent", "#708090"},
        {"border", "#506070"}, {"success", "#112233"}, {"warning", "#cc8800"},
        {"error", "#cc2233"}};
    if (optional) {
        result.insert("surface_input", "#223344");
        result.insert("surface_muted", "#334455");
        result.insert("surface_elevated", "#445566");
        result.insert("text_on_accent", "#f1f2f3");
        result.insert("focus_ring", "#a1b2c3");
    }
    return result;
}

QJsonObject fonts(bool swapped = false) {
    return {{"body", swapped ? "DejaVu Serif" : "DejaVu Sans"},
        {"display", swapped ? "DejaVu Sans" : "DejaVu Serif"},
        {"mono", "DejaVu Sans Mono"}};
}

QJsonObject themed(const QJsonObject &palette, const QJsonValue &families = QJsonValue::Null,
                  int revision = 1) {
    QJsonArray nodes{QJsonObject{{"Input", QJsonObject{{"binding_id", "theme-input"},
        {"label", "Prepared input"}, {"value", "Keep selected text"},
        {"input_kind", "text"}, {"enabled", true}}}},
        QJsonObject{{"Text", QJsonObject{{"content", "Heading"}, {"style", "heading"}}}},
        QJsonObject{{"Text", QJsonObject{{"content", "Code"}, {"style", "monospace"}}}}};
    auto command = surface("first", revision, "Prepared title", nodes);
    auto payload = command.value("ReplaceSurface").toObject();
    auto value = payload.value("surface").toObject();
    QJsonObject tokens{{"minimum_target_size", 48}, {"corner_radius", 12},
        {"spacing_small", 4}, {"spacing_medium", 8}, {"spacing_large", 16}, {"colors", palette}};
    tokens.insert("fonts", families);
    value.insert("tokens", tokens);
    payload.insert("surface", value);
    command.insert("ReplaceSurface", payload);
    return command;
}

QLineEdit *input() {
    auto *field = window()->findChild<QLineEdit *>("theme-input");
    assert(field);
    return field;
}

QLabel *label(const QString &text) {
    for (auto *item : window()->findChildren<QLabel *>())
        if (item->text() == text) return item;
    assert(false);
    return nullptr;
}

class GeometryReady : public QObject {
public:
    QWidget *widget;
    QEventLoop completed;
    bool eventFilter(QObject *, QEvent *event) override {
        if (event->type() == QEvent::Resize && widget->width() > 0 && widget->height() > 0)
            completed.quit();
        return false;
    }
};

void verifyColors(bool optional) {
    assert(QApplication::palette().color(QPalette::Window) == QColor("#102030"));
    assert(input()->palette().color(QPalette::Text) == QColor("#e0e1e2"));
    const QColor base(optional ? "#223344" : "#203040");
    assert(input()->palette().color(QPalette::Base) == base);
    if (input()->width() == 0 || input()->height() == 0) {
        GeometryReady ready;
        ready.widget = input();
        input()->installEventFilter(&ready);
        ready.completed.exec();
    }
    const auto pixels = input()->grab().toImage();
    assert(!pixels.isNull());
    assert(pixels.pixelColor(pixels.width() - 8, pixels.height() / 2) == base);
    assert(QApplication::palette().color(QPalette::AlternateBase)
        == QColor(optional ? "#334455" : "#203040"));
    assert(QApplication::palette().color(QPalette::ToolTipBase)
        == QColor(optional ? "#445566" : "#203040"));
    assert(QApplication::palette().color(QPalette::HighlightedText)
        == QColor(optional ? "#f1f2f3" : "#102030"));
}

// Feature: generic_presentation_protocol.feature — render the supplied theme roles.
QList<Step> appearance() {
    return {{{themed(colors(), fonts()), profile("first")}, [] {
        verifyColors(true);
        assert(input()->font().family() == "DejaVu Sans");
        assert(label("Heading")->font().family() == "DejaVu Serif");
        assert(label("Code")->font().family() == "DejaVu Sans Mono");
        assert(events.isEmpty());
    }}};
}

QList<Step> focusedUpdate() {
    auto changed = colors(); changed.insert("background", "#314159");
    return {{{themed(colors(), fonts()), profile("first")}, [] {
        verifyColors(true);
        keptInput = input();
        input()->setFocus();
        input()->setSelection(5, 8);
        assert(input()->selectedText() == "selected");
        events = {};
    }}, {{themed(changed, fonts(true))}, [] {
        assert(keptInput && input() == keptInput);
        assert(input()->hasFocus());
        assert(input()->selectionStart() == 5);
        assert(input()->selectedText() == "selected");
        assert(input()->cursorPosition() == 13);
        assert(input()->font().family() == "DejaVu Serif");
        assert(label("Heading")->font().family() == "DejaVu Sans");
        assert(QApplication::palette().color(QPalette::Window) == QColor("#314159"));
        assert(events.isEmpty());
    }}};
}

QList<Step> fallback() {
    auto absent = colors(false);
    for (const char *role : {"surface_input", "surface_muted", "surface_elevated", "text_on_accent", "focus_ring"})
        absent.insert(role, QJsonValue::Null);
    return {{{themed(absent), profile("first")}, [] {
        verifyColors(false);
        assert(events.isEmpty());
    }}};
}

QList<Step> reload() {
    return {{{themed(colors(), fonts()), profile("first")}, [] {
        verifyColors(true);
        keptInput = input();
        input()->setFocus();
        input()->setSelection(5, 8);
        QPalette altered = QApplication::palette();
        altered.setColor(QPalette::Window, QColor("#abcdef"));
        QApplication::setPalette(altered);
        assert(QApplication::palette().color(QPalette::Window) == QColor("#abcdef"));
        events = {};
    }}, {{"ReloadTheme"}, [] {
        assert(keptInput && input() == keptInput);
        verifyColors(true);
        assert(input()->hasFocus());
        assert(input()->selectedText() == "selected");
        assert(events.isEmpty());
    }}};
}

QList<Step> invalid() {
    auto bad = colors(); bad.insert("accent", "#ffffff; } QLineEdit { color: red; }");
    auto optional = colors(); optional.insert("surface_input", "red");
    return {{{themed(colors(), fonts(), 9), profile("first")}, [] { verifyColors(true); }},
        {{themed(bad, fonts(), 9), themed(optional, fonts(), 9),
          themed(colors(false), fonts(), 8)}, [] {
            verifyColors(true);
            assert(events.isEmpty());
        }}};
}

QList<Step> unsafeFont() {
    auto supplied = fonts();
    supplied.insert("body", "Injected\"; } QLineEdit#theme-input { color: #ff0000; } QWidget { font-family: \"");
    return {{{themed(colors(), supplied), profile("first")}, [] {
        verifyColors(true);
        assert(input()->palette().color(QPalette::Text) != QColor("#ff0000"));
        assert(events.isEmpty());
    }}};
}

void setup() { schedule(); }
}
Q_COREAPP_STARTUP_FUNCTION(setup)

int main(int argc, char **argv) {
    qunsetenv("QT_DISPLAY_SNAPSHOT");
    const QString name = argc > 1 ? QString::fromUtf8(argv[1]) : "appearance";
    if (name == "appearance") steps = appearance();
    else if (name == "focus") steps = focusedUpdate();
    else if (name == "fallback") steps = fallback();
    else if (name == "reload") steps = reload();
    else if (name == "invalid") steps = invalid();
    else if (name == "unsafe_font") steps = unsafeFont();
    else return 2;
    const int result = qt_display_start(argc, argv, receive, &events);
    assert(result == 0);
    return result;
}
