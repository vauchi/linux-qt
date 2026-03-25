<!-- SPDX-FileCopyrightText: 2026 Mattia Egloff <mattia.egloff@pm.me> -->
<!-- SPDX-License-Identifier: GPL-3.0-or-later -->

# qVauchi (Qt) — Design Inventory

## Components (17 types)

All components are rendered by `src/coreui/componentrenderer.cpp` via `ComponentRenderer::render()`.
Each component maps a vauchi-core JSON `Component` variant to Qt6 widgets.

| # | Component | File | Qt Widget(s) | Interactive | Used On |
|---|-----------|------|-------------|-------------|---------|
| 1 | Text | `textcomponent.cpp` | `QLabel` | No | All screens (titles, descriptions) |
| 2 | TextInput | `textinputcomponent.cpp` | `QLineEdit` | Yes (TextChanged on Enter/focus-leave) | Onboarding, Settings |
| 3 | PinInput | `pininputcomponent.cpp` | `QLineEdit` (password echo) | Yes (TextChanged) | Lock, DuressPin |
| 4 | ToggleList | `togglelistcomponent.cpp` | `QCheckBox` (multiple) | Yes (ItemToggled) | Onboarding (groups), Exchange |
| 5 | ContactList | `contactlistcomponent.cpp` | `QListWidget` | Yes (ListItemSelected) | Contacts |
| 6 | FieldList | `fieldlistcomponent.cpp` | `QListWidget` with headers | Yes (ListItemSelected) | MyInfo, ContactDetail |
| 7 | CardPreview | `cardpreviewcomponent.cpp` | `QFrame` + `QTabWidget` | Yes (GroupViewSelected) | MyInfo, ContactDetail |
| 8 | QrCode | `qrcodecomponent.cpp` | `QLabel` (QPixmap via libqrencode) | Yes (scan via CameraBackend, paste fallback) | Exchange |
| 9 | ConfirmationDialog | `confirmationdialogcomponent.cpp` | `QPushButton` (confirm/cancel) | Yes (ActionPressed) | EmergencyShred |
| 10 | InfoPanel | `infopanelcomponent.cpp` | `QLabel` + `QVBoxLayout` | No | Help, Support |
| 11 | StatusIndicator | `statusindicatorcomponent.cpp` | `QLabel` with icon | No | DeliveryStatus |
| 12 | ActionList | `actionlistcomponent.cpp` | `QPushButton` list | Yes (ActionPressed) | Settings, Backup |
| 13 | SettingsGroup | `settingsgroupcomponent.cpp` | `QGroupBox` with toggles | Yes (ActionPressed, ItemToggled) | Settings |
| 14 | Divider | `dividercomponent.cpp` | `QFrame` (HLine) | No | Various |

| 15 | ShowToast | `showtoastcomponent.cpp` | `QWidget` + `QLabel` + `QPushButton` | Yes (UndoPressed) | Post-delete, post-save |
| 16 | InlineConfirm | `inlineconfirmcomponent.cpp` | `QFrame` + `QPushButton` (confirm/cancel) | Yes (ActionPressed) | EmergencyShred |
| 17 | EditableText | `editabletextcomponent.cpp` | `QLabel` ↔ `QLineEdit` + `QPushButton` | Yes (TextChanged, ActionPressed) | MyInfo (name editing) |
| 18 | Banner | `bannercomponent.cpp` | `QLabel` + `QPushButton` (horizontal) | Yes (ActionPressed) | Informational bar with optional action |

## Screens (12 navigable via CABI)

Navigation via sidebar `QListWidget`. Screen data from `vauchi_app_navigate_to()` → JSON ScreenModel → `ScreenRenderer::render()`.

| # | Screen | CABI Name | Sidebar Label | Key Components |
|---|--------|-----------|---------------|----------------|
| 1 | Home/MyInfo | `home` / `my_info` | "Home" / "My info" | CardPreview, FieldList, ActionList |
| 2 | Contacts | `contacts` | "Contacts" | ContactList |
| 3 | Exchange | `exchange` | "Exchange" | QrCode, Text |
| 4 | Settings | `settings` | "Settings" | SettingsGroup, ActionList |
| 5 | Help | `help` | "Help" | InfoPanel, ActionList |
| 6 | Backup | `backup` | "Backup" | ActionList, TextInput |
| 7 | Lock | `lock` | "Lock" | PinInput, Text |
| 8 | Onboarding | `onboarding` | "Onboarding" | TextInput, ToggleList |
| 9 | EmergencyShred | `emergency_shred` | "Emergency shred" | ConfirmationDialog, Text |
| 10 | DeviceLinking | `device_linking` | "Device linking" | QrCode, ActionList |
| 11 | DuressPin | `duress_pin` | "Duress pin" | PinInput, ConfirmationDialog |
| 12 | DeliveryStatus | `delivery_status` | "Delivery status" | StatusIndicator |

### Missing Screens (5 — present in GTK, absent from Qt CABI)

| Screen | GTK Label | Why Missing |
|--------|-----------|-------------|
| Sync | "Sync" | Not exposed in CABI `vauchi_app_navigate_to()` |
| Recovery | "Recovery" | Not exposed in CABI |
| Groups | "Groups" | Not exposed in CABI |
| Privacy | "Privacy" | Not exposed in CABI |
| Support | "Support" | Not exposed in CABI |

## Workflows (5)

### W1: Onboarding
```
[No Identity] → Onboarding screen (via CABI workflow engine)
  → Enter name (TextInput)
  → Complete → Home screen
```
**Note:** Qt uses `VauchiWorkflow` (separate from AppEngine) for onboarding. Fewer steps than GTK onboarding.

### W2: Contact Exchange
```
Exchange screen → Show QR (QrCode display)
  → Hardware dispatch: QrRequestScan
  → If camera available: CameraBackend opens scan dialog (zbar QR decode)
  → If no camera or scan cancelled: paste dialog fallback (QInputDialog)
  → Exchange complete → contact added
```
**Note:** Camera scanning via Qt Multimedia + zbar. Paste fallback via `promptQrPaste()` in ScreenRenderer.

### W3: Contact Management
```
Contacts → Select contact (ContactList)
  → Contact detail (CardPreview, FieldList)
  → Action handling via CABI
```

### W4: Backup
```
Settings → Backup
  → Export/Import via ActionList buttons
  → Password entry (TextInput)
```

### W5: Settings
```
Settings → SettingsGroup items
  → Navigate to sub-screens (DuressPin, DeviceLinking, etc.)
```

## Hardware Integration

| Hardware | Module | Feature Flag | Detection | Status |
|----------|--------|-------------|-----------|--------|
| Camera | `platform/camerabackend.cpp` | `VAUCHI_HAS_CAMERA` | `QMediaDevices::videoInputs()` | QR scanning via Qt Multimedia + zbar |
| BLE | `platform/blebackend.cpp` + `bleadvertiser.cpp` | `VAUCHI_HAS_BLUETOOTH` | `/sys/class/bluetooth/` | Qt Bluetooth discovery + GATT read/write + BlueZ D-Bus advertising |
| Audio | `platform/audiobackend.cpp` | `VAUCHI_HAS_AUDIO` | `QMediaDevices` | Ultrasonic FSK (Goertzel decode, 44.1kHz PCM) on worker thread |
| NFC | `platform/nfcbackend.cpp` | `VAUCHI_HAS_NFC` | `SCardEstablishContext` | PC/SC exchange via pcsclite (SELECT AID + EXCHANGE APDU) on worker thread |

All hardware backends are optional (CMake feature flags). When unavailable, `HardwareBackend` sends `HardwareUnavailable` events to core.

## Platform Features

| Feature | Status | Notes |
|---------|--------|-------|
| System Tray | Yes | `QSystemTrayIcon` with Show/Quit actions |
| Menu Bar | Yes | File (Quit) + Help (About Vauchi) |
| Keyboard Shortcuts | **Minimal** | Only Quit action, no navigation keys |
| Window Title | "Vauchi" | Fixed, no dynamic subtitle |

## Accessibility Status

**Current: AT-SPI labels set on all components.** Every component uses `setObjectName()` for test identification and `setAccessibleName()` for screen reader support. Key coverage:

- Navigation sidebar: `setAccessibleName("Navigation")`
- Screen title: `setAccessibleName(screen["title"])`
- All list widgets: `setAccessibleName("Contacts")`, `setAccessibleName("Fields")`, `setAccessibleName("Actions")`
- Text inputs: `setAccessibleName(data["label"])`
- QR code: `setAccessibleName("QR code for contact exchange")` / `"Scan QR code"`
- Settings groups: `setAccessibleName(data["label"])`
- All other components: `setAccessibleName(title)` or `setAccessibleName(warning)`

AT-SPI tests in `tests/atspi/` verify the accessibility tree.

## Known Issues

1. **Missing 5 screens** — CABI `vauchi_app_navigate_to()` doesn't expose Sync, Recovery, Groups, Privacy, Support.
2. **No keyboard navigation** — No sidebar keyboard shortcuts, no screen-level hotkeys.
