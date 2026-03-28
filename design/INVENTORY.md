<!-- SPDX-FileCopyrightText: 2026 Mattia Egloff <mattia.egloff@pm.me> -->
<!-- SPDX-License-Identifier: GPL-3.0-or-later -->

# qVauchi (Qt) — Design Inventory

## Components (16 types)

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
| 9 | InfoPanel | `infopanelcomponent.cpp` | `QLabel` + `QVBoxLayout` | No | Help, Support |
| 10 | StatusIndicator | `statusindicatorcomponent.cpp` | `QLabel` with icon | No | DeliveryStatus |
| 11 | ActionList | `actionlistcomponent.cpp` | `QPushButton` list | Yes (ActionPressed) | Settings, Backup, More |
| 12 | SettingsGroup | `settingsgroupcomponent.cpp` | `QGroupBox` with toggles | Yes (ActionPressed, ItemToggled) | Settings |
| 13 | InlineConfirm | `inlineconfirmcomponent.cpp` | `QFrame` + `QPushButton` (confirm/cancel) | Yes (ActionPressed) | EmergencyShred |
| 14 | EditableText | `editabletextcomponent.cpp` | `QLabel` ↔ `QLineEdit` + `QPushButton` | Yes (TextChanged, ActionPressed) | MyInfo (name editing) |
| 15 | Divider | `dividercomponent.cpp` | `QFrame` (HLine) | No | Various |
| 16 | Banner | `bannercomponent.cpp` | `QLabel` + `QPushButton` (horizontal) | Yes (ActionPressed) | Informational bar with optional action |

## Navigation

### Sidebar (5 top-level screens)

Built dynamically from `vauchi_app_available_screens()`. When no identity exists, only Onboarding appears.

| # | Screen | CABI Name | Key Components |
|---|--------|-----------|----------------|
| 1 | My Info | `my_info` | CardPreview, FieldList, ActionList |
| 2 | Contacts | `contacts` | ContactList |
| 3 | Exchange | `exchange` | QrCode, Text |
| 4 | Groups | `groups` | ToggleList, ActionList |
| 5 | More | `more` | ActionList (navigation hub) |

### More screen sub-navigation

The More screen renders an ActionList linking to secondary screens.
All are reached via `vauchi_app_navigate_to()`.

| Target | CABI Name | Key Components |
|--------|-----------|----------------|
| Sync | `sync` | StatusIndicator, ActionList |
| Devices | `device_linking` | QrCode, ActionList |
| Settings | `settings` | SettingsGroup, ActionList |
| Backup | `backup` | ActionList, TextInput |
| Privacy | `privacy` | SettingsGroup, ActionList |
| Help | `help` | InfoPanel, ActionList |

### Action-navigated sub-screens

Reached via ActionResult navigation (e.g., tapping a contact):

| Screen | CABI Name | Reached From |
|--------|-----------|--------------|
| Contact Detail | `contact_detail` | Contacts (select item) |
| Contact Edit | `contact_edit` | Contact Detail (edit action) |
| Contact Visibility | `contact_visibility` | Contact Detail (visibility action) |
| Entry Detail | `entry_detail` | MyInfo (tap field) |
| Group Detail | `group_detail` | Groups (select item) |
| Lock | `lock` | Settings (password set) |
| Duress PIN | `duress_pin` | Settings (action) |
| Emergency Shred | `emergency_shred` | Settings (action) |
| Delivery Status | `delivery_status` | Settings/More (action) |
| Recovery | `recovery` | Settings (action) |
| Support | `support` | Help (action) |
| Onboarding | `onboarding` | No identity exists |

## Workflows (5)

### W1: Onboarding

```text
[No Identity] → Onboarding screen (via CABI workflow engine)
  → Enter name (TextInput)
  → Complete → Home screen
```

### W2: Contact Exchange

```text
Exchange screen → Show QR (QrCode display)
  → Hardware dispatch: QrRequestScan
  → If camera available: CameraBackend opens scan dialog (zbar QR decode)
  → If no camera or scan cancelled: paste dialog fallback (QInputDialog)
  → Exchange complete → contact added
```

### W3: Contact Management

```text
Contacts → Select contact (ContactList)
  → Contact detail (CardPreview, FieldList)
  → Action handling via CABI
```

### W4: Backup

```text
More → Backup
  → Export/Import via ActionList buttons
  → Password entry (TextInput)
```

### W5: Settings

```text
More → Settings → SettingsGroup items
  → Navigate to sub-screens (DuressPin, DeviceLinking, etc.)
```

## Hardware Integration

| Hardware | Module | Feature Flag | Detection | Status |
|----------|--------|-------------|-----------|--------|
| Camera | `platform/camerabackend.cpp` | `VAUCHI_HAS_CAMERA` | `QMediaDevices::videoInputs()` | QR scanning via Qt Multimedia + zbar |
| BLE | `platform/blebackend.cpp` + `bleadvertiser.cpp` | `VAUCHI_HAS_BLUETOOTH` | `/sys/class/bluetooth/` | Qt Bluetooth discovery + GATT read/write + BlueZ D-Bus advertising |
| Audio | `platform/audiobackend.cpp` | `VAUCHI_HAS_AUDIO` | `QMediaDevices` | Ultrasonic FSK (Goertzel decode, 44.1kHz PCM) on worker thread |
| NFC | `platform/nfcbackend.cpp` | `VAUCHI_HAS_NFC` | `SCardEstablishContext` | PC/SC exchange via pcsclite (SELECT AID + EXCHANGE APDU) on worker thread |

All hardware backends are optional (CMake feature flags).
When unavailable, `HardwareBackend` sends
`HardwareUnavailable` events to core.

## Platform Features

| Feature | Status | Notes |
|---------|--------|-------|
| System Tray | Yes | `QSystemTrayIcon` with Show/Quit actions |
| Menu Bar | Yes | File (Quit) + Help (About Vauchi) |
| Keyboard Shortcuts | Yes | Alt+1..5 for sidebar, Ctrl+Q for quit |
| Window Title | "Vauchi" | Fixed, no dynamic subtitle |

## Accessibility Status

**AT-SPI labels set on all components.** Every component uses
`setObjectName()` for test identification and
`setAccessibleName()` for screen reader support. Key coverage:

- Navigation sidebar: `setAccessibleName("Navigation")`
- Screen title: `setAccessibleName(screen["title"])`
- All list widgets: `setAccessibleName("Contacts")`, `setAccessibleName("Fields")`, `setAccessibleName("Actions")`
- Text inputs: `setAccessibleName(data["label"])`
- QR code: `setAccessibleName("QR code for contact exchange")` / `"Scan QR code"`
- Settings groups: `setAccessibleName(data["label"])`
- All other components: `setAccessibleName(title)` or `setAccessibleName(warning)`

AT-SPI tests in `tests/atspi/` verify the accessibility tree.

## Keyboard Shortcuts

| Shortcut | Action |
|----------|--------|
| Alt+1..5 | Navigate to sidebar screen (My Card, Contacts, Exchange, Groups, More) |
| Ctrl+Q | Quit |
| Alt | Activate menu bar |
