// SPDX-FileCopyrightText: 2026 Mattia Egloff <mattia.egloff@pm.me>
// SPDX-License-Identifier: GPL-3.0-or-later

use std::{cell::Cell, rc::Rc};

// @internal — host callbacks may own thread-local engine and hardware state.
fn main() {
    assert_eq!(qt_display::render("[]"), Err(qt_display::RenderError));
    let polled = Rc::new(Cell::new(0));
    let observed = Rc::new(Cell::new(false));
    let environment = Rc::new(Cell::new(false));
    let reported = environment.clone();
    let events = observed.clone();
    let ticks = polled.clone();
    let args = vec![
        "qt-display-poll-test".to_owned(),
        "-platform".to_owned(),
        "offscreen".to_owned(),
    ];
    let result = qt_display::run_with_poll(
        &args,
        Box::new(move |event| {
            assert!([
                r#"{"ApplicationMenuAvailabilityChanged":{"available":true}}"#,
                r#"{"FilePickerCapabilitiesChanged":{"correlated_results":true}}"#,
                r#"{"PresentationEnvironmentChanged":{"available_height":700,"available_width":900,"input_modes":["pointer","keyboard"],"motion":"full"}}"#,
            ]
            .contains(&event));
            events.set(true);
            if event.contains("PresentationEnvironmentChanged") {
                reported.set(true);
            }
        }),
        Box::new(move || {
            ticks.set(ticks.get() + 1);
            assert_eq!(qt_display::render("[]"), Ok(()));
            if ticks.get() == 2 {
                qt_display::stop();
            }
        }),
    );
    assert_eq!(result, 0);
    assert_eq!(polled.get(), 2);
    assert!(observed.get());
    assert!(environment.get());
    assert_eq!(Rc::strong_count(&polled), 1);
    assert_eq!(Rc::strong_count(&observed), 1);
    assert_eq!(qt_display::render("[]"), Err(qt_display::RenderError));
}
