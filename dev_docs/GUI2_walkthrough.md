# Walkthrough: forge.gui Phase 2 + NMEA Terminal App

## Summary

Extended `forge.gui` with 3 new widget types and built a complete NMEA serial terminal app in FORGE.

## Phase 2: New Widgets Added

| Widget | API | Description |
|--------|-----|-------------|
| **Scrollable Text Log** | [log_create](file:///home/chuck/Dropbox/Programming/Languages_and_Code/Programming_Projects/Programming_Tools/LANGUAGES/FORGE/runtime/forge_gui.c#604-606), [log_add](file:///home/chuck/Dropbox/Programming/Languages_and_Code/Programming_Projects/Programming_Tools/LANGUAGES/FORGE/runtime/forge_gui.c#358-376), [log_clear](file:///home/chuck/Dropbox/Programming/Languages_and_Code/Programming_Projects/Programming_Tools/LANGUAGES/FORGE/runtime/forge_gui.c#377-384), [log_draw](file:///home/chuck/Dropbox/Programming/Languages_and_Code/Programming_Projects/Programming_Tools/LANGUAGES/FORGE/runtime/forge_gui.c#608-609), [log_count](file:///home/chuck/Dropbox/Programming/Languages_and_Code/Programming_Projects/Programming_Tools/LANGUAGES/FORGE/runtime/forge_gui.c#462-466) | Ring-buffer backed (4096 lines), mouse-wheel scrolling, scissor-clipped, scrollbar indicator, auto-scroll |
| **Dropdown** | [dropdown(x, y, w, h, items, selected)](file:///home/chuck/Dropbox/Programming/Languages_and_Code/Programming_Projects/Programming_Tools/LANGUAGES/FORGE/runtime/forge_gui.c#498-515) → [int](file:///home/chuck/Dropbox/Programming/Languages_and_Code/Programming_Projects/Programming_Tools/LANGUAGES/FORGE/src/interp/interp.c#177-181) | raygui `GuiDropdownBox` wrapper with persistent edit state, semicolon-separated items |
| **Color Button** | [color_button(x, y, w, h, text, bg_rgba, tx_rgba)](file:///home/chuck/Dropbox/Programming/Languages_and_Code/Programming_Projects/Programming_Tools/LANGUAGES/FORGE/runtime/forge_gui.c#611-612) → `bool` | Custom-colored button with hover effect, centered text |

## NMEA Terminal App

[nmea_terminal.fg](file:///home/chuck/Dropbox/Programming/Languages_and_Code/Programming_Projects/Programming_Tools/LANGUAGES/FORGE/examples/nmea_terminal.fg) — modeled after PyTerm, features:

- **Scrollable NMEA data log** with green colorization for valid NMEA sentences
- **Port open/close** with color-changing toggle button
- **Port/baud configuration** via textbox and dropdown
- **Pause receive** toggle
- **NMEA identifier tracking** sidebar
- **TX/RX LED indicators** with flash animation
- **Command input** box with Send CMD button
- **Status bar** showing port, baud, status, line count

## Files Modified (Phase 2)

| File | Change |
|------|--------|
| [forge_gui.h](file:///home/chuck/Dropbox/Programming/Languages_and_Code/Programming_Projects/Programming_Tools/LANGUAGES/FORGE/runtime/forge_gui.h) | Added declarations for log, dropdown, color_button |
| [forge_gui.c](file:///home/chuck/Dropbox/Programming/Languages_and_Code/Programming_Projects/Programming_Tools/LANGUAGES/FORGE/runtime/forge_gui.c) | Added implementations (~230 lines) + stubs |
| [checker.c](file:///home/chuck/Dropbox/Programming/Languages_and_Code/Programming_Projects/Programming_Tools/LANGUAGES/FORGE/src/typecheck/checker.c) | Added return types for new functions |
| [interp.c](file:///home/chuck/Dropbox/Programming/Languages_and_Code/Programming_Projects/Programming_Tools/LANGUAGES/FORGE/src/interp/interp.c) | Added dispatch for new functions |

## Files Created (Phase 2)

| File | Purpose |
|------|---------|
| [nmea_terminal.fg](file:///home/chuck/Dropbox/Programming/Languages_and_Code/Programming_Projects/Programming_Tools/LANGUAGES/FORGE/examples/nmea_terminal.fg) | NMEA serial terminal app (~210 lines of FORGE) |

## Build Verification

All files compile cleanly with zero warnings:
```
forge_gui.c   ✅
interp.c      ✅
checker.c     ✅
```

## How to Run

```bash
make clean && make GUI=1
./forge run examples/nmea_terminal.fg
```

Then open a virtual serial port (e.g., with socat or BRIDGE) and connect.
