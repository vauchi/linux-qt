# qVauchi (Qt) — Design Inventory

## Components (14 types)

All components are rendered by `src/coreui/componentrenderer.cpp` via `ComponentRenderer::render()`.
Each component maps a vauchi-core JSON `Component` variant to Qt6 widgets.

| # | Component | File | Qt Widget(s) | Interactive | Used On |
|---|-----------|------|-------------|-------------|---------|
| 1 | Text | `textcomponent.cpp` | `QLabel` | No | All screens (titles, descriptions) |
| 2 | TextInput | `textinputcomponent.cpp` | `QLineEdit` | Yes (TextChanged on **every keystroke**) | Onboarding, Settings |
| 3 | PinInput | `pininputcomponent.cpp` | `QLineEdit` (password echo) | Yes (TextChanged) | Lock, DuressPin |
| 4 | ToggleList | `togglelistcomponent.cpp` | `QCheckBox` (multiple) | Yes (ItemToggled) | Onboarding (groups), Exchange |
| 5 | ContactList | `contactlistcomponent.cpp` | `QListWidget` | Yes (ListItemSelected) | Contacts |
| 6 | FieldList | `fieldlistcomponent.cpp` | `QListWidget` with headers | Yes (ListItemSelected) | MyInfo, ContactDetail |
| 7 | CardPreview | `cardpreviewcomponent.cpp` | `QFrame` + `QTabWidget` | Yes (GroupViewSelected) | MyInfo, ContactDetail |
| 8 | QrCode | `qrcodecomponent.cpp` | `QLabel` (QPixmap via libqrencode) | No (display only) | Exchange |
| 9 | ConfirmationDialog | `confirmationdialogcomponent.cpp` | `QPushButton` (confirm/cancel) | Yes (ActionPressed) | EmergencyShred |
| 10 | InfoPanel | `infopanelcomponent.cpp` | `QLabel` + `QVBoxLayout` | No | Help, Support |
| 11 | StatusIndicator | `statusindicatorcomponent.cpp` | `QLabel` with icon | No | DeliveryStatus |
| 12 | ActionList | `actionlistcomponent.cpp` | `QPushButton` list | Yes (ActionPressed) | Settings, Backup |
| 13 | SettingsGroup | `settingsgroupcomponent.cpp` | `QGroupBox` with toggles | Yes (ActionPressed, ItemToggled) | Settings |
| 14 | Divider | `dividercomponent.cpp` | `QFrame` (HLine) | No | Various |

### Missing Components (3 — present in GTK, absent from Qt)

| Component | GTK Implementation | Impact |
|-----------|-------------------|--------|
| **ShowToast** | `adw::Toast` via ToastOverlay | No toast notifications in Qt — user gets no feedback on save/delete |
| **InlineConfirm** | Warning box + confirm/cancel buttons | No inline confirmation — emergency shred may lack safety prompt |
| **EditableText** | Label ↔ Entry toggle | No inline name editing — must use separate edit screen |

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

### Missing Screens (6 — present in GTK, absent from Qt CABI)

| Screen | GTK Label | Why Missing |
|--------|-----------|-------------|
| Sync | "Sync" | Not exposed in CABI `vauchi_app_navigate_to()` |
| TorSettings | "Tor Settings" | Not exposed in CABI |
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
  → "Camera not available on desktop" message
  → QR paste not implemented (no scan dialog)
  → Exchange via relay only
```
**Note:** Qt has no camera integration and no paste dialog fallback.

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

| Hardware | Status | Notes |
|----------|--------|-------|
| Camera | **Not implemented** | QrCode shows "Camera not available on desktop" |
| BLE | **Not implemented** | No BlueZ integration |
| Audio | **Not implemented** | No CPAL/audio integration |
| NFC | **Not implemented** | No integration |

## Platform Features

| Feature | Status | Notes |
|---------|--------|-------|
| System Tray | Yes | `QSystemTrayIcon` with Show/Quit actions |
| Menu Bar | Yes | File (Quit) + Help (About Qt) |
| Keyboard Shortcuts | **Minimal** | Only Quit action, no navigation keys |
| Window Title | "Vauchi" | Fixed, no dynamic subtitle |

## Accessibility Status

**Current: No accessibility labels set.** No `setObjectName()`, no `setAccessibleName()`, no `setAccessibleDescription()` on any widget. Invisible to AT-SPI screen readers.

**Needed for AT-SPI testing:** Every widget needs `setObjectName()` for test identification and `setAccessibleName()` for screen reader support.

## Known Issues

1. **TextInput fires on every keystroke** — `QLineEdit::textChanged` triggers per-character, causing screen re-render per keystroke. GTK only fires on Enter/focus-leave.
2. **Missing 3 component types** — ShowToast, InlineConfirm, EditableText not handled in `componentrenderer.cpp`; unknown types fall through to Divider.
3. **Missing 6 screens** — CABI `vauchi_app_navigate_to()` doesn't expose Sync, TorSettings, Recovery, Groups, Privacy, Support.
4. **No QR paste fallback** — Unlike GTK which has a paste dialog for QR data, Qt only displays QR codes.
5. **No keyboard navigation** — No sidebar keyboard shortcuts, no screen-level hotkeys.
