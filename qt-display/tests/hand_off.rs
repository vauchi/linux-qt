// SPDX-FileCopyrightText: 2026 Mattia Egloff <mattia.egloff@pm.me>
// SPDX-License-Identifier: GPL-3.0-or-later

// @internal — an app asks before it starts its engine; with no link, or no
// running shell to take it, the launch carries on (private#1029, #650).
fn main() {
    std::env::set_var("VAUCHI_DEEP_LINK_SERVER", "vauchi-qt-rust-hand-off-test");
    let with_link = [
        "qvauchi".to_owned(),
        "vauchi://exchange?pk=AAAA&n=BBBB".to_owned(),
    ];
    assert!(!qt_display::hand_off_links(&with_link));
    let without_link = ["qvauchi".to_owned(), "--flag".to_owned()];
    assert!(!qt_display::hand_off_links(&without_link));
}
