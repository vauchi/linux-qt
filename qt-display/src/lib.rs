// SPDX-FileCopyrightText: 2026 Mattia Egloff <mattia.egloff@pm.me>
// SPDX-License-Identifier: GPL-3.0-or-later
//! The Qt display shell behind three calls (ADR-078 step 2.2, D1 B).
//!
//! The app owns the engine and the main thread: [`run`] hands the thread
//! to Qt until [`stop`] or the window closes, [`render`] posts a batch of
//! contract `Command`s to it from any thread, and every user action comes
//! back as a JSON `Event` through the callback given to [`run`].
use std::ffi::{c_char, c_int, c_void, CStr, CString};

mod ffi {
    use super::*;
    pub type EventCallback = unsafe extern "C" fn(*const c_char, *mut c_void);
    extern "C" {
        pub fn qt_display_start(
            argc: c_int,
            argv: *mut *mut c_char,
            on_event: EventCallback,
            user_data: *mut c_void,
        ) -> c_int;
        pub fn qt_display_render(commands_json: *const c_char, len: usize) -> c_int;
        pub fn qt_display_stop();
    }
}

/// Receives each event the shell reports, JSON-encoded.
pub type EventSink = Box<dyn FnMut(&str) + Send>;

unsafe extern "C" fn trampoline(event_json: *const c_char, user_data: *mut c_void) {
    // SAFETY: user_data is the Box<EventSink> that `run` keeps alive for
    // the whole event loop, and event_json is valid for this call.
    let sink = &mut *(user_data as *mut EventSink);
    if let Ok(text) = CStr::from_ptr(event_json).to_str() {
        sink(text);
    }
}

/// Runs the shell on the calling thread; returns Qt's exit code.
pub fn run(args: &[String], on_event: EventSink) -> i32 {
    let mut argv: Vec<CString> = args
        .iter()
        .map(|a| CString::new(a.as_str()).expect("argument without NUL"))
        .collect();
    let mut argv_ptrs: Vec<*mut c_char> =
        argv.iter_mut().map(|a| a.as_ptr() as *mut c_char).collect();
    let mut sink: EventSink = on_event;
    // SAFETY: argv outlives the call; the sink outlives the event loop.
    unsafe {
        ffi::qt_display_start(
            argv_ptrs.len() as c_int,
            argv_ptrs.as_mut_ptr(),
            trampoline,
            &mut sink as *mut EventSink as *mut c_void,
        )
    }
}

/// Renders a JSON array of contract `Command`s on the GUI thread.
/// Fails while no shell runs or when the text is not a JSON array.
pub fn render(commands_json: &str) -> Result<(), ()> {
    let c = CString::new(commands_json).map_err(|_| ())?;
    // SAFETY: the bytes are valid for the call; the shell copies them.
    let rc = unsafe { ffi::qt_display_render(c.as_ptr(), commands_json.len()) };
    if rc == 0 {
        Ok(())
    } else {
        Err(())
    }
}

/// Asks the running shell to quit; [`run`] then returns.
pub fn stop() {
    // SAFETY: no arguments; a no-op while no shell runs.
    unsafe { ffi::qt_display_stop() }
}
