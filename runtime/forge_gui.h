/*
 * FORGE Runtime Library — GUI Module (raylib + raygui)
 *
 * Provides windowing, drawing, input, and immediate-mode widgets.
 * Compile with -DFORGE_HAS_GUI and link -lraylib to enable.
 * Without FORGE_HAS_GUI, all functions print an error and exit.
 */

#ifndef FORGE_GUI_H
#define FORGE_GUI_H

#include <stdint.h>
#include "forge_runtime.h"  /* For forge_str_t */

/* ═══════════════════════════════════════════════════════════════════════════
 * Window Management
 * ═══════════════════════════════════════════════════════════════════════════ */

/* Initialize window with given dimensions and title */
void forge_gui_init_window(int64_t width, int64_t height, forge_str_t title);

/* Close the window and clean up */
void forge_gui_close_window(void);

/* Returns 1 if window should remain open, 0 if close requested */
int forge_gui_window_open(void);

/* Set target frames per second */
void forge_gui_set_target_fps(int64_t fps);

/* Get current frames per second */
int64_t forge_gui_get_fps(void);

/* Get delta time (seconds since last frame) */
double forge_gui_get_dt(void);

/* ═══════════════════════════════════════════════════════════════════════════
 * Drawing Control
 * ═══════════════════════════════════════════════════════════════════════════ */

/* Begin drawing frame */
void forge_gui_begin_draw(void);

/* End drawing frame (presents to screen) */
void forge_gui_end_draw(void);

/* Clear background with RGBA color */
void forge_gui_clear(int64_t r, int64_t g, int64_t b, int64_t a);

/* ═══════════════════════════════════════════════════════════════════════════
 * Shape Drawing
 * ═══════════════════════════════════════════════════════════════════════════ */

/* Draw a line between two points */
void forge_gui_draw_line(int64_t x1, int64_t y1, int64_t x2, int64_t y2,
                         int64_t r, int64_t g, int64_t b, int64_t a);

/* Draw a filled rectangle */
void forge_gui_draw_rect(int64_t x, int64_t y, int64_t w, int64_t h,
                         int64_t r, int64_t g, int64_t b, int64_t a);

/* Draw a rectangle outline */
void forge_gui_draw_rect_lines(int64_t x, int64_t y, int64_t w, int64_t h,
                               int64_t r, int64_t g, int64_t b, int64_t a);

/* Draw a filled circle */
void forge_gui_draw_circle(int64_t cx, int64_t cy, double radius,
                           int64_t r, int64_t g, int64_t b, int64_t a);

/* Draw a circle outline */
void forge_gui_draw_circle_lines(int64_t cx, int64_t cy, double radius,
                                 int64_t r, int64_t g, int64_t b, int64_t a);

/* ═══════════════════════════════════════════════════════════════════════════
 * Text
 * ═══════════════════════════════════════════════════════════════════════════ */

/* Draw text at position with given font size and color */
void forge_gui_draw_text(forge_str_t text, int64_t x, int64_t y,
                         int64_t size, int64_t r, int64_t g, int64_t b, int64_t a);

/* Measure text width in pixels for given font size */
int64_t forge_gui_measure_text(forge_str_t text, int64_t size);

/* ═══════════════════════════════════════════════════════════════════════════
 * Input — Keyboard
 * ═══════════════════════════════════════════════════════════════════════════ */

/* Returns 1 if key was pressed this frame */
int forge_gui_is_key_pressed(int64_t key);

/* Returns 1 if key is currently held down */
int forge_gui_is_key_down(int64_t key);

/* Returns 1 if key was released this frame */
int forge_gui_is_key_released(int64_t key);

/* Get the last key pressed (0 if none) */
int64_t forge_gui_get_key_pressed(void);

/* ═══════════════════════════════════════════════════════════════════════════
 * Input — Mouse
 * ═══════════════════════════════════════════════════════════════════════════ */

/* Get mouse X position */
int64_t forge_gui_mouse_x(void);

/* Get mouse Y position */
int64_t forge_gui_mouse_y(void);

/* Returns 1 if mouse button was pressed this frame */
int forge_gui_is_mouse_pressed(int64_t button);

/* Returns 1 if mouse button is currently held down */
int forge_gui_is_mouse_down(int64_t button);

/* ═══════════════════════════════════════════════════════════════════════════
 * Widgets (raygui immediate-mode)
 * ═══════════════════════════════════════════════════════════════════════════ */

/* Draw a button, returns 1 if clicked */
int forge_gui_button(int64_t x, int64_t y, int64_t w, int64_t h, forge_str_t text);

/* Draw a label (static text in a box) */
void forge_gui_label(int64_t x, int64_t y, int64_t w, int64_t h, forge_str_t text);

/* Draw a checkbox, returns new checked state */
int forge_gui_checkbox(int64_t x, int64_t y, int64_t w, int64_t h,
                       forge_str_t text, int checked);

/* Draw a slider, returns new value */
double forge_gui_slider(int64_t x, int64_t y, int64_t w, int64_t h,
                        double min_val, double max_val, double value);

/* Draw a text box, returns (possibly modified) text */
forge_str_t forge_gui_textbox(int64_t x, int64_t y, int64_t w, int64_t h,
                              forge_str_t text, int64_t max_len);

/* Set dark visual style */
void forge_gui_set_style_dark(void);

/* Set light visual style */
void forge_gui_set_style_light(void);

/* ═══════════════════════════════════════════════════════════════════════════
 * Font Management
 *
 * Load a TTF/OTF font file into a slot (0–7). Call after init_window().
 * The size parameter sets the base render quality (20–48 recommended).
 * Returns the slot id on success, or -1 on failure.
 * ═══════════════════════════════════════════════════════════════════════════ */

/* Load a font from a file path. size=0 uses 32 as default. Returns slot id (0-7) or -1. */
int64_t forge_gui_load_font(forge_str_t path, int64_t size);

/* Set the active font used by draw_text, measure_text, log panels, and color_button.
 * Pass id=-1 to revert to the raylib default font. */
void forge_gui_set_font(int64_t id);

/* Unload a font slot and free GPU memory. Reverts to default if it was active. */
void forge_gui_unload_font(int64_t id);

/* ═══════════════════════════════════════════════════════════════════════════
 * Scrollable Text Log
 *
 * A ring-buffer backed scrollable text display with per-line RGBA colors.
 * Supports up to 8 independent log instances identified by integer ID.
 * ═══════════════════════════════════════════════════════════════════════════ */

/* Create/initialize a log panel with the given ID (0-7) and max visible lines */
void forge_gui_log_create(int64_t id, int64_t x, int64_t y, int64_t w, int64_t h,
                          int64_t max_lines, int64_t font_size);

/* Add a colored text line to the log */
void forge_gui_log_add(int64_t id, forge_str_t text,
                       int64_t r, int64_t g, int64_t b, int64_t a);

/* Clear all lines from the log */
void forge_gui_log_clear(int64_t id);

/* Draw the log panel (call each frame inside begin_draw/end_draw) */
void forge_gui_log_draw(int64_t id);

/* Get the number of lines currently in the log */
int64_t forge_gui_log_count(int64_t id);

/* ═══════════════════════════════════════════════════════════════════════════
 * Dropdown Selector
 * ═══════════════════════════════════════════════════════════════════════════ */

/* Draw a dropdown box. items is semicolon-separated, e.g. "9600;19200;38400".
 * Returns the newly selected index. */
int64_t forge_gui_dropdown(int64_t x, int64_t y, int64_t w, int64_t h,
                           forge_str_t items, int64_t selected);

/* ═══════════════════════════════════════════════════════════════════════════
 * Color Button (toggle-style)
 * ═══════════════════════════════════════════════════════════════════════════ */

/* Draw a colored button with custom background color, returns 1 if clicked */
int forge_gui_color_button(int64_t x, int64_t y, int64_t w, int64_t h,
                           forge_str_t text,
                           int64_t bg_r, int64_t bg_g, int64_t bg_b, int64_t bg_a,
                           int64_t tx_r, int64_t tx_g, int64_t tx_b, int64_t tx_a);

#endif /* FORGE_GUI_H */

