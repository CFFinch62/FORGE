# FORGE GUI Library Guide

**Module:** `forge.gui`  
**Build Requirement:** `make GUI=1`  
**Backend:** raylib + raygui (vendored — no external install needed)

> This guide teaches you how to build graphical applications with FORGE, from
> opening your first window to building a full interactive app. It assumes you
> already know FORGE basics (variables, procedures, loops).

---

## Table of Contents

1. [Building with GUI Support](#1-building-with-gui-support)
2. [Your First Window](#2-your-first-window)
3. [The Render Loop](#3-the-render-loop)
4. [Drawing Shapes](#4-drawing-shapes)
5. [Drawing Text](#5-drawing-text)
6. [Handling Input](#6-handling-input)
7. [Widgets](#7-widgets)
8. [Scrollable Text Log](#8-scrollable-text-log)
9. [Color Constants](#9-color-constants)
10. [Complete API Reference](#10-complete-api-reference)
11. [Example: Hello GUI](#11-example-hello-gui)
12. [Example: NMEA Terminal](#12-example-nmea-terminal)

---

## 1. Building with GUI Support

FORGE includes raylib and raygui in its `vendor/` folder, so no external
installation is needed. Just build with the `GUI=1` flag:

```bash
make clean
make GUI=1
```

This produces a `forge` binary that can run `.fg` files using `forge.gui`.

**Without `GUI=1`**, the `forge` binary still works for all non-GUI programs.
If a program tries to call `gui.*` functions in a non-GUI build, it prints an
error and exits:

```
FORGE: GUI support not compiled. Rebuild with: make GUI=1
```

**System requirements (Linux):** OpenGL, X11, and pthread development headers.
On Debian/Ubuntu, these are usually pre-installed. If not:

```bash
sudo apt-get install libgl1-mesa-dev libx11-dev
```

---

## 2. Your First Window

Every GUI program follows the same pattern:

```forge
import forge.gui

proc main() -> void:
    gui.init_window(800, 600, "My First Window")
    gui.set_target_fps(60)

    while gui.window_open():
        gui.begin_draw()
        gui.clear(30, 30, 35, 255)
        gui.draw_text("Hello, GUI!", 300, 280, 24, 255, 255, 255, 255)
        gui.end_draw()

    gui.close_window()
```

**What each line does:**

| Line | Purpose |
|------|---------|
| `gui.init_window(w, h, title)` | Creates a window of the given size with a title bar |
| `gui.set_target_fps(60)` | Limits the render loop to 60 frames per second |
| `gui.window_open()` | Returns `true` until the user clicks the close button |
| `gui.begin_draw()` | Starts a new frame — must be called before any drawing |
| `gui.clear(r, g, b, a)` | Fills the window with a background color |
| `gui.end_draw()` | Finishes the frame and displays it on screen |
| `gui.close_window()` | Cleans up and destroys the window |

**Run it:**
```bash
./forge run my_first_window.fg
```

---

## 3. The Render Loop

GUI programs are **not** like console programs. Instead of running once and
exiting, they run in a **loop** that redraws the screen every frame (typically
60 times per second).

```forge
while gui.window_open():
    gui.begin_draw()

    # 1. Clear the screen
    gui.clear(30, 30, 35, 255)

    # 2. Draw everything
    gui.draw_text("Frame: " + str(gui.get_fps()), 10, 10, 20, 255, 255, 255, 255)

    # 3. Handle input (check keys, mouse, buttons)
    if gui.is_key_pressed(256):    # 256 = Escape key
        break

    gui.end_draw()
```

**Key concept:** Everything you draw only lasts for one frame. On the next
frame, you clear the screen and draw everything again. This is called
**immediate-mode rendering**.

---

## 4. Drawing Shapes

All drawing functions take RGBA color values (0–255 each):

```forge
# Filled rectangle
gui.draw_rect(x, y, width, height, r, g, b, a)

# Rectangle outline
gui.draw_rect_lines(x, y, width, height, r, g, b, a)

# Filled circle
gui.draw_circle(center_x, center_y, radius, r, g, b, a)

# Circle outline
gui.draw_circle_lines(center_x, center_y, radius, r, g, b, a)

# Line between two points
gui.draw_line(x1, y1, x2, y2, r, g, b, a)
```

**Example — draw a red rectangle with a green circle:**

```forge
gui.draw_rect(100, 100, 200, 150, 200, 50, 50, 255)
gui.draw_circle(200, 175, 50.0, 50, 200, 50, 255)
```

---

## 5. Drawing Text

```forge
gui.draw_text(text, x, y, font_size, r, g, b, a)
```

**Parameters:**
- `text` — the string to display
- `x, y` — pixel position (top-left corner of text)
- `font_size` — size in pixels (14 = small, 20 = normal, 30 = large)
- `r, g, b, a` — color (RGBA, 0–255)

**Measuring text width** (useful for centering):

```forge
var width: int = gui.measure_text("Hello", 20)
var centered_x: int = (800 - width) / 2
gui.draw_text("Hello", centered_x, 300, 20, 255, 255, 255, 255)
```

---

## 6. Handling Input

### Keyboard

```forge
# Was the key pressed THIS frame? (one-shot, for menus/toggles)
if gui.is_key_pressed(key_code):
    # do something once

# Is the key being HELD DOWN? (for continuous movement)
if gui.is_key_down(key_code):
    # do something every frame

# Was the key released THIS frame?
if gui.is_key_released(key_code):
    # do something once

# Get the last key pressed (0 if none)
var key: int = gui.get_key_pressed()
```

**Common key codes:**

| Key | Code | Key | Code |
|-----|------|-----|------|
| Escape | 256 | Enter | 257 |
| Space | 32 | Backspace | 259 |
| Up | 265 | Down | 264 |
| Left | 263 | Right | 262 |
| A–Z | 65–90 | 0–9 | 48–57 |

### Mouse

```forge
var mx: int = gui.mouse_x()
var my: int = gui.mouse_y()

if gui.is_mouse_pressed(0):    # 0 = left button
    # clicked this frame

if gui.is_mouse_down(0):       # held down
    # dragging
```

---

## 7. Widgets

Widgets are interactive GUI elements. They are drawn AND checked in a single
call — this is called **immediate-mode GUI**.

### Button

```forge
if gui.button(x, y, width, height, "Click Me"):
    # This code runs when the button is clicked
    print("Button was clicked!")
```

### Color Button

A button with custom background and text colors (great for toggle states):

```forge
# gui.color_button(x, y, w, h, text, bg_r,g,b,a, text_r,g,b,a)
if gui.color_button(10, 10, 100, 30, "Active", 0,180,0,255, 0,0,0,255):
    # clicked
```

### Label

Static text inside a box (styled by raygui theme):

```forge
gui.label(x, y, width, height, "Status: Ready")
```

### Checkbox

```forge
var checked: bool = false

# Inside render loop:
checked = gui.checkbox(x, y, 20, 20, "Enable logging", checked)
```

### Slider

```forge
var volume: float = 0.5

# Inside render loop:
volume = gui.slider(x, y, 200, 20, 0.0, 1.0, volume)
```

### Text Box

An editable text input field. Click to activate, type to edit:

```forge
var name: str = ""

# Inside render loop:
name = gui.textbox(x, y, 200, 30, name, 128)
# 128 = maximum character length
```

### Dropdown

A dropdown selector. Items are semicolon-separated:

```forge
var baud_idx: int = 0

# Inside render loop:
baud_idx = gui.dropdown(x, y, 120, 30, "4800;9600;19200;38400", baud_idx)
```

**Note:** The dropdown opens on click and closes on selection. The return value
is the index of the selected item (0-based).

---

## 8. Scrollable Text Log

The **log widget** is a scrollable, colored text display — perfect for
terminal output, chat messages, or data feeds.

### Creating a Log

```forge
# gui.log_create(id, x, y, width, height, max_lines, font_size)
gui.log_create(0, 10, 50, 780, 400, 4096, 16)
```

- `id` — A number 0–7 identifying this log (you can have up to 8)
- `max_lines` — Maximum lines in the buffer (older lines are discarded)
- `font_size` — Text size in pixels

### Adding Lines

```forge
# gui.log_add(id, text, r, g, b, a)
gui.log_add(0, "System started", 0, 255, 100, 255)    # green
gui.log_add(0, "WARNING: low memory", 255, 200, 0, 255)  # yellow
gui.log_add(0, "ERROR: file not found", 255, 60, 60, 255) # red
```

### Drawing the Log

Call this every frame inside your render loop:

```forge
gui.log_draw(0)
```

The log automatically:
- Scrolls to the bottom when new lines arrive
- Supports mouse-wheel scrolling to view history
- Shows a scrollbar when content overflows
- Clips text to the panel boundaries

### Other Log Functions

```forge
gui.log_clear(0)                    # Remove all lines
var n: int = gui.log_count(0)       # Get number of lines
```

---

## 9. Color Constants

FORGE GUI uses RGBA values (0–255). Here are useful presets:

| Color | R | G | B | A |
|-------|---|---|---|---|
| Black | 0 | 0 | 0 | 255 |
| White | 255 | 255 | 255 | 255 |
| Red | 255 | 60 | 60 | 255 |
| Green | 0 | 200 | 0 | 255 |
| Blue | 60 | 120 | 255 | 255 |
| Yellow | 255 | 255 | 0 | 255 |
| Cyan | 0 | 255 | 255 | 255 |
| Dark background | 30 | 30 | 35 | 255 |
| Light gray text | 180 | 180 | 180 | 255 |
| Transparent | 0 | 0 | 0 | 0 |

**Alpha channel:** 255 = fully opaque, 0 = fully transparent.

---

## 10. Complete API Reference

### Window Management

| Function | Returns | Description |
|----------|---------|-------------|
| `gui.init_window(w, h, title)` | void | Create a window |
| `gui.close_window()` | void | Destroy the window |
| `gui.window_open()` | bool | `true` until close is requested |
| `gui.set_target_fps(fps)` | void | Set frame rate limit |
| `gui.get_fps()` | int | Get current frames per second |
| `gui.get_dt()` | float | Seconds since last frame |

### Drawing Control

| Function | Returns | Description |
|----------|---------|-------------|
| `gui.begin_draw()` | void | Start a frame |
| `gui.end_draw()` | void | End a frame (present to screen) |
| `gui.clear(r, g, b, a)` | void | Fill background |

### Shapes

| Function | Returns | Description |
|----------|---------|-------------|
| `gui.draw_line(x1, y1, x2, y2, r, g, b, a)` | void | Draw a line |
| `gui.draw_rect(x, y, w, h, r, g, b, a)` | void | Filled rectangle |
| `gui.draw_rect_lines(x, y, w, h, r, g, b, a)` | void | Rectangle outline |
| `gui.draw_circle(cx, cy, radius, r, g, b, a)` | void | Filled circle |
| `gui.draw_circle_lines(cx, cy, radius, r, g, b, a)` | void | Circle outline |

### Text

| Function | Returns | Description |
|----------|---------|-------------|
| `gui.draw_text(text, x, y, size, r, g, b, a)` | void | Draw text |
| `gui.measure_text(text, size)` | int | Get text width in pixels |

### Input — Keyboard

| Function | Returns | Description |
|----------|---------|-------------|
| `gui.is_key_pressed(key)` | bool | Key pressed this frame |
| `gui.is_key_down(key)` | bool | Key currently held |
| `gui.is_key_released(key)` | bool | Key released this frame |
| `gui.get_key_pressed()` | int | Last key pressed (0 = none) |

### Input — Mouse

| Function | Returns | Description |
|----------|---------|-------------|
| `gui.mouse_x()` | int | Mouse X position |
| `gui.mouse_y()` | int | Mouse Y position |
| `gui.is_mouse_pressed(btn)` | bool | Button pressed this frame |
| `gui.is_mouse_down(btn)` | bool | Button currently held |

### Widgets

| Function | Returns | Description |
|----------|---------|-------------|
| `gui.button(x, y, w, h, text)` | bool | Standard button |
| `gui.color_button(x, y, w, h, text, bg_rgba, tx_rgba)` | bool | Colored button |
| `gui.label(x, y, w, h, text)` | void | Static label |
| `gui.checkbox(x, y, w, h, text, checked)` | int | Checkbox toggle |
| `gui.slider(x, y, w, h, min, max, value)` | float | Value slider |
| `gui.textbox(x, y, w, h, text, max_len)` | str | Editable text input |
| `gui.dropdown(x, y, w, h, items, selected)` | int | Dropdown selector |

### Scrollable Text Log

| Function | Returns | Description |
|----------|---------|-------------|
| `gui.log_create(id, x, y, w, h, max, size)` | void | Create log panel |
| `gui.log_add(id, text, r, g, b, a)` | void | Add colored line |
| `gui.log_clear(id)` | void | Clear all lines |
| `gui.log_draw(id)` | void | Render the log panel |
| `gui.log_count(id)` | int | Number of lines |

### Style

| Function | Returns | Description |
|----------|---------|-------------|
| `gui.set_style_dark()` | void | Dark theme for raygui widgets |
| `gui.set_style_light()` | void | Default light theme |

---

## 11. Example: Hello GUI

A minimal program demonstrating a window, text, shapes, and a button:

```forge
import forge.gui

proc main() -> void:
    gui.init_window(640, 480, "Hello GUI")
    gui.set_target_fps(60)
    gui.set_style_dark()

    var clicks: int = 0

    while gui.window_open():
        gui.begin_draw()
        gui.clear(30, 30, 35, 255)

        # Title
        gui.draw_text("FORGE GUI Demo", 220, 30, 28, 255, 255, 255, 255)

        # Draw a colored rectangle
        gui.draw_rect(200, 100, 240, 80, 60, 120, 200, 255)
        gui.draw_text("A blue box", 260, 130, 20, 255, 255, 255, 255)

        # Button
        if gui.button(250, 250, 140, 40, "Click Me"):
            clicks = clicks + 1

        # Show click count
        gui.draw_text("Clicks: " + str(clicks), 270, 310, 20, 200, 200, 200, 255)

        gui.end_draw()

    gui.close_window()
```

---

## 12. Example: NMEA Terminal

See `examples/nmea_terminal.fg` for a complete, working NMEA 0183 serial
terminal built entirely in FORGE. It demonstrates:

- Scrollable log with colored text
- Serial port connection (using `forge.serial`)
- NMEA sentence validation (using `forge.nmea`)
- Toggle buttons, dropdown, textbox
- TX/RX activity indicators
- Status bar layout

Run it with:

```bash
./forge run examples/nmea_terminal.fg
```

---

*FORGE GUI Library Guide v0.1 — Fragillidae Software*
