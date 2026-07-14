// SPDX-FileCopyrightText: 2026 Mattia Egloff <mattia.egloff@pm.me>
// SPDX-License-Identifier: GPL-3.0-or-later

/// Tests for ComponentRenderer dispatch + the two new wire variants
/// shipped in vauchi-core 0.51.21 (Component::Indicator,
/// Component::SectionedActionList).
///
/// Plain `assert()` to match the rest of the linux-qt test suite
/// (smoke_test, app_engine_test, theme_test) — no Google Test
/// framework is wired in CMakeLists.

#include "../src/coreui/componentrenderer.h"
#include "../src/coreui/thememanager.h"

#include <QApplication>
#include <QHBoxLayout>
#include <QLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QList>
#include <QPushButton>
#include <QWidget>

#include <cassert>
#include <cstdio>
#include <string>

// ---- helpers -------------------------------------------------------

static QJsonObject parse(const char *json) {
    return QJsonDocument::fromJson(QByteArray(json)).object();
}

// Find the first descendant of `root` of type T whose objectName/text
// matches the predicate. Returns nullptr if none.
template <typename T, typename Pred>
static T *find(QWidget *root, Pred p) {
    if (auto *as = qobject_cast<T *>(root)) {
        if (p(as)) return as;
    }
    for (QObject *child : root->children()) {
        if (auto *w = qobject_cast<QWidget *>(child)) {
            if (T *hit = find<T>(w, p)) return hit;
        }
    }
    return nullptr;
}

// Collect every descendant button label in DFS order.
static QStringList buttonLabels(QWidget *root) {
    QStringList out;
    if (auto *btn = qobject_cast<QPushButton *>(root)) {
        out << btn->text();
    }
    for (QObject *child : root->children()) {
        if (auto *w = qobject_cast<QWidget *>(child)) {
            out << buttonLabels(w);
        }
    }
    return out;
}

// Collect every descendant label text in DFS order.
static QStringList labelTexts(QWidget *root) {
    QStringList out;
    if (auto *lbl = qobject_cast<QLabel *>(root)) {
        out << lbl->text();
    }
    for (QObject *child : root->children()) {
        if (auto *w = qobject_cast<QWidget *>(child)) {
            out << labelTexts(w);
        }
    }
    return out;
}

// ---- Indicator -----------------------------------------------------

static void test_indicator_active_with_action_is_button() {
    QJsonObject json = parse(R"({
        "Indicator": {
            "id": "sync",
            "label": "Synced 15:47",
            "kind": "Active",
            "action_id": "sync_now"
        }
    })");

    QString capturedActionId;
    QWidget *w = ComponentRenderer::render(json, [&](const QJsonObject &a) {
        if (a.contains("ActionPressed")) {
            capturedActionId = a["ActionPressed"].toObject()["action_id"].toString();
        }
    });
    assert(w != nullptr);
    assert(w->objectName() == QStringLiteral("sync"));

    // Tappable indicator is rendered as a QPushButton carrying the label.
    auto *btn = qobject_cast<QPushButton *>(w);
    assert(btn != nullptr);
    assert(btn->text() == QStringLiteral("Synced 15:47"));
    assert(btn->isEnabled());

    btn->click();
    assert(capturedActionId == QStringLiteral("sync_now"));

    delete w;
    printf("  PASS: indicator_active_with_action_is_button\n");
}

static void test_indicator_error_display_only_is_label() {
    QJsonObject json = parse(R"({
        "Indicator": {
            "id": "online",
            "label": "Offline",
            "kind": "Error"
        }
    })");

    bool callbackFired = false;
    QWidget *w = ComponentRenderer::render(json, [&](const QJsonObject &) {
        callbackFired = true;
    });
    assert(w != nullptr);
    assert(w->objectName() == QStringLiteral("online"));

    // Display-only (action_id absent) — must NOT be a QPushButton.
    assert(qobject_cast<QPushButton *>(w) == nullptr);

    // The visible label must show "Offline" somewhere in the widget tree.
    QStringList texts = labelTexts(w);
    bool found = false;
    for (const QString &t : texts) {
        if (t.contains(QStringLiteral("Offline"))) { found = true; break; }
    }
    assert(found);

    // Display-only must not fire the callback under any path.
    assert(!callbackFired);

    delete w;
    printf("  PASS: indicator_error_display_only_is_label\n");
}

static void test_indicator_all_kinds_render_without_crash() {
    const char *kinds[] = {"Active", "Error", "Neutral", "Busy"};
    for (const char *k : kinds) {
        std::string js = std::string(R"({"Indicator":{"id":"x","label":"L","kind":")") + k + R"("}})";
        QJsonObject json = parse(js.c_str());
        QWidget *w = ComponentRenderer::render(json, nullptr);
        assert(w != nullptr);
        assert(w->objectName() == QStringLiteral("x"));
        // All kinds must emit a non-empty stylesheet snippet via ThemeManager.
        // We do not assert specific colors — that's ThemeManager's contract.
        delete w;
    }
    printf("  PASS: indicator_all_kinds_render_without_crash\n");
}

static void test_indicator_unknown_kind_renders_fallback_not_crash() {
    // Forward-compat: a future kind that we don't recognise must
    // degrade gracefully — render the label, no crash, no callback.
    QJsonObject json = parse(R"({
        "Indicator": {
            "id": "future",
            "label": "Some New State",
            "kind": "TotallyNewKind"
        }
    })");
    QWidget *w = ComponentRenderer::render(json, nullptr);
    assert(w != nullptr);
    QStringList texts = labelTexts(w);
    bool found = false;
    for (const QString &t : texts) {
        if (t.contains(QStringLiteral("Some New State"))) { found = true; break; }
    }
    assert(found);
    delete w;
    printf("  PASS: indicator_unknown_kind_renders_fallback_not_crash\n");
}

static void test_indicator_a11y_label_overrides_visible_label() {
    QJsonObject json = parse(R"({
        "Indicator": {
            "id": "sync",
            "label": "Synced 15:47",
            "kind": "Active",
            "action_id": "sync_now",
            "a11y": {"label": "Sync status: up to date", "hint": "Activate to sync now"}
        }
    })");
    QWidget *w = ComponentRenderer::render(json, [](const QJsonObject &) {});
    assert(w != nullptr);
    assert(w->accessibleName() == QStringLiteral("Sync status: up to date"));
    assert(w->accessibleDescription() == QStringLiteral("Activate to sync now"));
    delete w;
    printf("  PASS: indicator_a11y_label_overrides_visible_label\n");
}

// ---- SectionedActionList -------------------------------------------

static void test_sectioned_action_list_renders_section_headers_and_items() {
    QJsonObject json = parse(R"({
        "SectionedActionList": {
            "id": "more",
            "sections": [
                {
                    "id": "primary",
                    "label": "Primary",
                    "items": [
                        {"id": "settings", "label": "Settings", "icon": null, "detail": null},
                        {"id": "about", "label": "About", "icon": null, "detail": null}
                    ]
                },
                {
                    "id": "data",
                    "label": "Data",
                    "items": [
                        {"id": "export", "label": "Export", "icon": null, "detail": null}
                    ]
                }
            ]
        }
    })");

    QString capturedComponentId, capturedItemId;
    QWidget *w = ComponentRenderer::render(json, [&](const QJsonObject &a) {
        if (a.contains("ListItemSelected")) {
            auto inner = a["ListItemSelected"].toObject();
            capturedComponentId = inner["component_id"].toString();
            capturedItemId = inner["item_id"].toString();
        }
    });
    assert(w != nullptr);
    assert(w->objectName() == QStringLiteral("more"));

    // Section header labels must appear as QLabel text in the tree.
    QStringList labels = labelTexts(w);
    bool foundPrimary = false, foundData = false;
    for (const QString &t : labels) {
        if (t.contains(QStringLiteral("Primary"))) foundPrimary = true;
        if (t.contains(QStringLiteral("Data"))) foundData = true;
    }
    assert(foundPrimary);
    assert(foundData);

    // All three items must be rendered as buttons.
    QStringList buttons = buttonLabels(w);
    bool foundSettings = false, foundAbout = false, foundExport = false;
    for (const QString &t : buttons) {
        if (t == QStringLiteral("Settings")) foundSettings = true;
        if (t == QStringLiteral("About")) foundAbout = true;
        if (t == QStringLiteral("Export")) foundExport = true;
    }
    assert(foundSettings);
    assert(foundAbout);
    assert(foundExport);

    // Click the second item — it must fire ListItemSelected with the
    // outer SectionedActionList id and the item id.
    QPushButton *aboutBtn = find<QPushButton>(w, [](QPushButton *b) {
        return b->text() == QStringLiteral("About");
    });
    assert(aboutBtn != nullptr);
    aboutBtn->click();
    assert(capturedComponentId == QStringLiteral("more"));
    assert(capturedItemId == QStringLiteral("about"));

    delete w;
    printf("  PASS: sectioned_action_list_renders_section_headers_and_items\n");
}

static void test_sectioned_action_list_empty_sections_renders_empty() {
    QJsonObject json = parse(R"({
        "SectionedActionList": {
            "id": "empty",
            "sections": []
        }
    })");
    QWidget *w = ComponentRenderer::render(json, nullptr);
    assert(w != nullptr);
    assert(w->objectName() == QStringLiteral("empty"));
    // No buttons at all.
    assert(buttonLabels(w).isEmpty());
    delete w;
    printf("  PASS: sectioned_action_list_empty_sections_renders_empty\n");
}

static void test_sectioned_action_list_section_with_no_items() {
    QJsonObject json = parse(R"({
        "SectionedActionList": {
            "id": "more",
            "sections": [
                {"id": "primary", "label": "Primary", "items": []}
            ]
        }
    })");
    QWidget *w = ComponentRenderer::render(json, nullptr);
    assert(w != nullptr);
    // Header label is still rendered even though items is empty.
    QStringList labels = labelTexts(w);
    bool found = false;
    for (const QString &t : labels) {
        if (t.contains(QStringLiteral("Primary"))) { found = true; break; }
    }
    assert(found);
    assert(buttonLabels(w).isEmpty());
    delete w;
    printf("  PASS: sectioned_action_list_section_with_no_items\n");
}

static void test_sectioned_action_list_item_with_detail_renders_detail() {
    QJsonObject json = parse(R"({
        "SectionedActionList": {
            "id": "more",
            "sections": [{
                "id": "secondary",
                "label": "Secondary",
                "items": [
                    {"id": "version", "label": "Version", "icon": null, "detail": "0.51.21"}
                ]
            }]
        }
    })");
    QWidget *w = ComponentRenderer::render(json, nullptr);
    assert(w != nullptr);
    QStringList labels = labelTexts(w);
    bool foundDetail = false;
    for (const QString &t : labels) {
        if (t.contains(QStringLiteral("0.51.21"))) { foundDetail = true; break; }
    }
    assert(foundDetail);
    delete w;
    printf("  PASS: sectioned_action_list_item_with_detail_renders_detail\n");
}

// ---- Row (horizontal container) ------------------------------------

static void test_row_renders_children_horizontally_with_equal_stretch() {
    // Row is a horizontal container: each child is rendered via the same
    // dispatcher and given equal stretch so a child that fills its own
    // max width shares the row instead of overlapping its siblings.
    QJsonObject json = parse(R"({
        "Row": {
            "id": "exchange_preview_row",
            "items": [
                {"Text": {"id": "a", "content": "left"}},
                {"Text": {"id": "b", "content": "right"}}
            ]
        }
    })");
    QWidget *w = ComponentRenderer::render(json, nullptr);
    assert(w != nullptr);
    assert(w->objectName() == QStringLiteral("exchange_preview_row"));

    auto *box = qobject_cast<QHBoxLayout *>(w->layout());
    assert(box != nullptr);
    assert(box->count() == 2);
    // Both children carry equal stretch (1) so neither overflows.
    assert(box->stretch(0) == 1);
    assert(box->stretch(1) == 1);
    delete w;
    printf("  PASS: row_renders_children_horizontally_with_equal_stretch\n");
}

static void test_row_nested_does_not_crash() {
    // A Row may contain any Component, including another Row (recursive
    // dispatch). Empty inner items list is also tolerated.
    QJsonObject json = parse(R"({
        "Row": {
            "id": "outer",
            "items": [
                {"Row": {"id": "inner", "items": []}},
                {"Text": {"id": "t", "content": "x"}}
            ]
        }
    })");
    QWidget *w = ComponentRenderer::render(json, nullptr);
    assert(w != nullptr);
    auto *box = qobject_cast<QHBoxLayout *>(w->layout());
    assert(box != nullptr);
    assert(box->count() == 2);
    delete w;
    printf("  PASS: row_nested_does_not_crash\n");
}

// ---- forward-compat unknown variants -------------------------------

static void test_unknown_component_variant_does_not_crash() {
    // The renderer already emits a "Unknown component: X" placeholder
    // QLabel for unrecognised variants. Lock that contract so a future
    // refactor doesn't silently revert to a null return / segfault.
    QJsonObject json = parse(R"({"FutureWidget":{"id":"x","label":"y"}})");
    QWidget *w = ComponentRenderer::render(json, nullptr);
    assert(w != nullptr);
    auto *lbl = qobject_cast<QLabel *>(w);
    assert(lbl != nullptr);
    assert(lbl->text().contains(QStringLiteral("FutureWidget")));
    delete w;
    printf("  PASS: unknown_component_variant_does_not_crash\n");
}

// ---- PinInput -------------------------------------------------------

static void test_pin_input_uses_only_core_supplied_length() {
    QJsonObject supplied = parse(R"({
        "PinInput": {"id":"pin","label":"Code","length":4,
                     "filled":0,"masked":true,"validation_error":null}
    })");
    QWidget *w = ComponentRenderer::render(supplied, nullptr);
    assert(w != nullptr);
    auto *input = find<QLineEdit>(w, [](QLineEdit *) { return true; });
    assert(input != nullptr);
    assert(input->maxLength() == 4);
    delete w;

    QJsonObject omitted = parse(R"({
        "PinInput": {"id":"pin","label":"Code","filled":0,
                     "masked":true,"validation_error":null}
    })");
    w = ComponentRenderer::render(omitted, nullptr);
    assert(w != nullptr);
    input = find<QLineEdit>(w, [](QLineEdit *) { return true; });
    assert(input != nullptr);
    assert(input->maxLength() != 6);
    delete w;
    printf("  PASS: pin_input_uses_only_core_supplied_length\n");
}

// ---- core-owned generic actions -----------------------------------

static void test_inline_confirm_forwards_opaque_action_ids() {
    QJsonObject json = parse(R"({
        "InlineConfirm": {
            "id":"visual-only-id", "warning":"Continue?",
            "confirm_text":"Yes", "cancel_text":"No",
            "confirm_action_id":"opaque/confirm#7",
            "cancel_action_id":"opaque/cancel#8", "destructive":false
        }
    })");
    QStringList captured;
    QWidget *w = ComponentRenderer::render(json, [&](const QJsonObject &a) {
        captured << a["ActionPressed"].toObject()["action_id"].toString();
    });
    assert(w != nullptr);

    auto *cancel = find<QPushButton>(w, [](QPushButton *b) {
        return b->text() == QStringLiteral("No");
    });
    auto *confirm = find<QPushButton>(w, [](QPushButton *b) {
        return b->text() == QStringLiteral("Yes");
    });
    assert(cancel != nullptr && confirm != nullptr);
    cancel->click();
    confirm->click();
    assert(captured == QStringList({QStringLiteral("opaque/cancel#8"),
                                    QStringLiteral("opaque/confirm#7")}));
    delete w;
    printf("  PASS: inline_confirm_forwards_opaque_action_ids\n");
}

static void test_editable_text_uses_core_copy_and_action_ids() {
    QJsonObject display = parse(R"({
        "EditableText": {
            "id":"visual-only-id", "label":"Note", "value":"Hello",
            "edit_text":"Change note", "save_text":"Keep it",
            "cancel_text":"Leave it", "edit_action_id":"opaque/edit#1",
            "save_action_id":"opaque/save#2",
            "cancel_action_id":"opaque/cancel#3", "editing":false
        }
    })");
    QStringList captured;
    QWidget *w = ComponentRenderer::render(display, [&](const QJsonObject &a) {
        if (a.contains("ActionPressed")) {
            captured << a["ActionPressed"].toObject()["action_id"].toString();
        }
    });
    auto *edit = find<QPushButton>(w, [](QPushButton *b) {
        return b->text() == QStringLiteral("Change note");
    });
    assert(edit != nullptr);
    edit->click();
    assert(captured == QStringList({QStringLiteral("opaque/edit#1")}));
    delete w;

    QJsonObject editing = display;
    QJsonObject inner = editing["EditableText"].toObject();
    inner["editing"] = true;
    editing["EditableText"] = inner;
    captured.clear();
    w = ComponentRenderer::render(editing, [&](const QJsonObject &a) {
        if (a.contains("ActionPressed")) {
            captured << a["ActionPressed"].toObject()["action_id"].toString();
        }
    });
    auto *cancel = find<QPushButton>(w, [](QPushButton *b) {
        return b->text() == QStringLiteral("Leave it");
    });
    auto *save = find<QPushButton>(w, [](QPushButton *b) {
        return b->text() == QStringLiteral("Keep it");
    });
    assert(cancel != nullptr && save != nullptr);
    cancel->click();
    save->click();
    assert(captured == QStringList({QStringLiteral("opaque/cancel#3"),
                                    QStringLiteral("opaque/save#2")}));
    delete w;
    printf("  PASS: editable_text_uses_core_copy_and_action_ids\n");
}

static void test_image_circle_forwards_opaque_edit_action_id() {
    QJsonObject json = parse(R"({
        "ImageCircle": {
            "id":"visual-only-id", "image_data":null, "initials":"AG",
            "brightness":0.0, "editable":true,
            "edit_action_id":"opaque/image-edit#4",
            "a11y":{"label":"Change profile image","hint":"Choose a new image"}
        }
    })");
    QString captured;
    QWidget *w = ComponentRenderer::render(json, [&](const QJsonObject &a) {
        captured = a["ActionPressed"].toObject()["action_id"].toString();
    });
    assert(w != nullptr);
    auto *edit = find<QPushButton>(w, [](QPushButton *b) {
        return b->objectName() == QStringLiteral("image-edit-affordance");
    });
    assert(edit != nullptr);
    assert(edit->accessibleName() == QStringLiteral("Change profile image"));
    edit->click();
    assert(captured == QStringLiteral("opaque/image-edit#4"));
    delete w;
    printf("  PASS: image_circle_forwards_opaque_edit_action_id\n");
}

static void test_status_indicator_renders_core_localized_label() {
    QJsonObject json = parse(R"({
        "StatusIndicator": {
            "id":"sync-status", "icon":null,
            "title":"Synchronisation", "detail":null,
            "status":"Success", "status_label":"Erfolg"
        }
    })");
    QWidget *w = ComponentRenderer::render(json, nullptr);
    assert(w != nullptr);
    auto *badge = find<QLabel>(w, [](QLabel *label) {
        return label->objectName() == QStringLiteral("status_label");
    });
    assert(badge != nullptr);
    assert(badge->text() == QStringLiteral("Erfolg"));
    assert(badge->accessibleName() == QStringLiteral("Erfolg"));
    delete w;
    printf("  PASS: status_indicator_renders_core_localized_label\n");
}

static void test_qr_code_renders_core_localized_label() {
    QJsonObject json = parse(R"({
        "QrCode": {
            "id":"peer-code", "data":"", "mode":"Scan",
            "label":"Code scannen",
            "a11y":{"label":"Code der anderen Person scannen"}
        }
    })");
    QWidget *w = ComponentRenderer::render(json, nullptr);
    assert(w != nullptr);
    assert(w->accessibleName() == QStringLiteral("Code der anderen Person scannen"));
    QStringList texts = labelTexts(w);
    assert(texts.contains(QStringLiteral("Code scannen")));
    assert(!texts.contains(QStringLiteral("Scan QR Code")));
    delete w;
    printf("  PASS: qr_code_renders_core_localized_label\n");
}

static void test_field_list_uses_core_localized_title() {
    QJsonObject json = parse(R"({
        "FieldList": {
            "id":"fields", "title":"Kontaktfelder", "fields":[],
            "visibility_mode":"ShowHide", "available_scopes":[]
        }
    })");
    QWidget *w = ComponentRenderer::render(json, nullptr);
    assert(w != nullptr);
    assert(w->accessibleName() == QStringLiteral("Kontaktfelder"));
    delete w;
    printf("  PASS: field_list_uses_core_localized_title\n");
}

// ---- main ----------------------------------------------------------

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    ThemeManager::applyDefaultTheme();

    printf("ComponentRenderer tests:\n");
    test_indicator_active_with_action_is_button();
    test_indicator_error_display_only_is_label();
    test_indicator_all_kinds_render_without_crash();
    test_indicator_unknown_kind_renders_fallback_not_crash();
    test_indicator_a11y_label_overrides_visible_label();
    test_sectioned_action_list_renders_section_headers_and_items();
    test_sectioned_action_list_empty_sections_renders_empty();
    test_sectioned_action_list_section_with_no_items();
    test_sectioned_action_list_item_with_detail_renders_detail();
    test_row_renders_children_horizontally_with_equal_stretch();
    test_row_nested_does_not_crash();
    test_unknown_component_variant_does_not_crash();
    test_pin_input_uses_only_core_supplied_length();
    test_inline_confirm_forwards_opaque_action_ids();
    test_editable_text_uses_core_copy_and_action_ids();
    test_image_circle_forwards_opaque_edit_action_id();
    test_status_indicator_renders_core_localized_label();
    test_qr_code_renders_core_localized_label();
    test_field_list_uses_core_localized_title();
    printf("All ComponentRenderer tests passed.\n");
    return 0;
}
