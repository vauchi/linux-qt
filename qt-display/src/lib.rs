// SPDX-FileCopyrightText: 2026 Mattia Egloff <mattia.egloff@pm.me>
// SPDX-License-Identifier: GPL-3.0-or-later
//! The Qt display shell behind four calls (ADR-078 step 2.2, D1 B).
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
        pub fn qt_display_start_with_poll(
            argc: c_int,
            argv: *mut *mut c_char,
            on_event: EventCallback,
            on_poll: Option<unsafe extern "C" fn(*mut c_void)>,
            user_data: *mut c_void,
        ) -> c_int;
        pub fn qt_display_render(commands_json: *const c_char, len: usize) -> c_int;
        pub fn qt_display_stop();
        pub fn qt_display_hand_off_links(argc: c_int, argv: *mut *mut c_char) -> c_int;
    }
}

/// Receives each event the shell reports, JSON-encoded.
pub type EventSink = Box<dyn FnMut(&str)>;

/// Drains app-owned observations on the calling thread without user input.
pub type PollSink = Box<dyn FnMut()>;

struct Callbacks {
    event: EventSink,
    poll: Option<PollSink>,
}

unsafe extern "C" fn trampoline(event_json: *const c_char, user_data: *mut c_void) {
    // SAFETY: run_callbacks keeps this state alive on Qt's thread until return.
    let callbacks = &mut *(user_data as *mut Callbacks);
    if let Ok(text) = CStr::from_ptr(event_json).to_str() {
        (callbacks.event)(text);
    }
}

unsafe extern "C" fn poll_trampoline(user_data: *mut c_void) {
    // SAFETY: Qt invokes both callbacks serially on run_callbacks' thread.
    let callbacks = &mut *(user_data as *mut Callbacks);
    if let Some(poll) = callbacks.poll.as_mut() {
        poll();
    }
}

/// Runs the shell on the calling thread; returns Qt's exit code.
pub fn run(args: &[String], on_event: EventSink) -> i32 {
    run_callbacks(args, on_event, None)
}

/// Runs Qt with a GUI-thread host poll, initially and every 50 ms.
/// The poll may render batches and stop the loop; it must not block the GUI.
pub fn run_with_poll(args: &[String], on_event: EventSink, on_poll: PollSink) -> i32 {
    run_callbacks(args, on_event, Some(on_poll))
}

fn run_callbacks(args: &[String], on_event: EventSink, on_poll: Option<PollSink>) -> i32 {
    let mut argv: Vec<CString> = args
        .iter()
        .map(|a| CString::new(a.as_str()).expect("argument without NUL"))
        .collect();
    let mut argv_ptrs: Vec<*mut c_char> =
        argv.iter_mut().map(|a| a.as_ptr() as *mut c_char).collect();
    let poll_callback = on_poll
        .as_ref()
        .map(|_| poll_trampoline as unsafe extern "C" fn(*mut c_void));
    let mut callbacks = Callbacks {
        event: on_event,
        poll: on_poll,
    };
    // SAFETY: argv and callbacks outlive Qt's loop; callbacks stay on this thread.
    unsafe {
        ffi::qt_display_start_with_poll(
            argv_ptrs.len() as c_int,
            argv_ptrs.as_mut_ptr(),
            trampoline,
            poll_callback,
            &mut callbacks as *mut Callbacks as *mut c_void,
        )
    }
}

/// The shell rejected a batch or was not running.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub struct RenderError;

impl std::fmt::Display for RenderError {
    fn fmt(&self, formatter: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        formatter.write_str("Qt display rejected the command batch")
    }
}

impl std::error::Error for RenderError {}

/// Renders a JSON array of contract `Command`s on the GUI thread.
/// Fails while no shell runs or when the text is not a JSON array.
pub fn render(commands_json: &str) -> Result<(), RenderError> {
    let c = CString::new(commands_json).map_err(|_| RenderError)?;
    // SAFETY: the bytes are valid for the call; the shell copies them.
    let rc = unsafe { ffi::qt_display_render(c.as_ptr(), commands_json.len()) };
    if rc == 0 {
        Ok(())
    } else {
        Err(RenderError)
    }
}

/// Hands this launch's `vauchi://` links to a running shell. Call it before
/// starting the engine: on `true` the running shell has them and this
/// launch should exit; `false` when there is no link or no running shell.
pub fn hand_off_links(args: &[String]) -> bool {
    let mut argv: Vec<CString> = args
        .iter()
        .map(|a| CString::new(a.as_str()).expect("argument without NUL"))
        .collect();
    let mut argv_ptrs: Vec<*mut c_char> =
        argv.iter_mut().map(|a| a.as_ptr() as *mut c_char).collect();
    // SAFETY: argv outlives the call; the shell copies what it keeps.
    unsafe { ffi::qt_display_hand_off_links(argv_ptrs.len() as c_int, argv_ptrs.as_mut_ptr()) == 1 }
}

/// Asks the running shell to quit; [`run`] then returns.
pub fn stop() {
    // SAFETY: no arguments; a no-op while no shell runs.
    unsafe { ffi::qt_display_stop() }
}
