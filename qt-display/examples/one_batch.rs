// SPDX-FileCopyrightText: 2026 Mattia Egloff <mattia.egloff@pm.me>
// SPDX-License-Identifier: GPL-3.0-or-later
//! The D1 spike's driver: renders the contract fixture's initial
//! ReplaceSurface and prints every event the shell reports.
//! `cargo run --example one_batch -- [--exit-after-ms N]`
fn main() {
    let fixture = concat!(
        env!("CARGO_MANIFEST_DIR"),
        "/../../contract/fixtures/presentation_contract_v1.json"
    );
    let text =
        std::fs::read_to_string(fixture).expect("the contract fixture beside this workspace");
    let start = text.find("\"initial_commands\"").expect("initial_commands")
        + "\"initial_commands\":".len();
    let batch = &text[start + text[start..].find('[').unwrap()..];
    let (mut depth, mut end) = (0usize, 0usize);
    for (i, ch) in batch.char_indices() {
        match ch {
            '[' => depth += 1,
            ']' => {
                depth -= 1;
                if depth == 0 {
                    end = i + 1;
                    break;
                }
            }
            _ => {}
        }
    }
    let batch = batch[..end].to_string();
    let args: Vec<String> = std::env::args().collect();
    let exit_after_ms = args
        .iter()
        .position(|a| a == "--exit-after-ms")
        .and_then(|i| args.get(i + 1))
        .and_then(|v| v.parse::<u64>().ok());
    std::thread::spawn(move || {
        std::thread::sleep(std::time::Duration::from_millis(300));
        qt_display::render(&batch).expect("the shell is running");
        if let Some(ms) = exit_after_ms {
            std::thread::sleep(std::time::Duration::from_millis(ms));
            qt_display::stop();
        }
    });
    std::process::exit(qt_display::run(
        &[args[0].clone()],
        Box::new(|event| println!("event {event}")),
    ));
}
