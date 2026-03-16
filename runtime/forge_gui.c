/*
 * FORGE Runtime Library — GUI Module (raylib + raygui)
 *
 * Provides windowing, drawing, input, and immediate-mode widgets.
 * When FORGE_HAS_GUI is defined, links against raylib.
 * Without it, all functions print an error and exit.
 */

#include "forge_gui.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef FORGE_HAS_GUI

#include "raylib.h"

#define RAYGUI_IMPLEMENTATION
#include "raygui.h"

/* ═══════════════════════════════════════════════════════════════════════════
 * Internal Helpers
 * ═══════════════════════════════════════════════════════════════════════════ */

/* Convert FORGE RGBA int64 components to raylib Color */
static Color rgba(int64_t r, int64_t g, int64_t b, int64_t a) {
    return (Color){
        (unsigned char)r, (unsigned char)g,
        (unsigned char)b, (unsigned char)a
    };
}

/* Null-terminate a forge_str_t for C APIs that need it.
 * Returns a pointer that MUST be freed if it differs from s.data. */
static const char* str_cstr(forge_str_t s, char* buf, int buf_size) {
    if (s.len < buf_size) {
        memcpy(buf, s.data, s.len);
        buf[s.len] = '\0';
        return buf;
    }
    char* heap = (char*)malloc(s.len + 1);
    memcpy(heap, s.data, s.len);
    heap[s.len] = '\0';
    return heap;
}

static void str_cstr_free(const char* ptr, forge_str_t s, const char* buf) {
    if (ptr != buf && ptr != s.data) {
        free((void*)ptr);
    }
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Textbox State Management
 * ═══════════════════════════════════════════════════════════════════════════
 *
 * raygui's GuiTextBox requires persistent state (edit mode toggle, char buffer).
 * We track up to MAX_TEXTBOXES instances keyed by (x, y) position.
 */

#define MAX_TEXTBOXES 32
#define TEXTBOX_BUF_SIZE 1024

typedef struct {
    int64_t x, y;
    bool edit_mode;
    char text[TEXTBOX_BUF_SIZE];
    int active;
} forge_textbox_state_t;

static forge_textbox_state_t g_textbox_states[MAX_TEXTBOXES];
static int g_textbox_count = 0;

static forge_textbox_state_t* get_textbox_state(int64_t x, int64_t y) {
    /* Look for existing */
    for (int i = 0; i < g_textbox_count; i++) {
        if (g_textbox_states[i].x == x && g_textbox_states[i].y == y) {
            return &g_textbox_states[i];
        }
    }
    /* Create new */
    if (g_textbox_count < MAX_TEXTBOXES) {
        forge_textbox_state_t* tb = &g_textbox_states[g_textbox_count++];
        tb->x = x;
        tb->y = y;
        tb->edit_mode = false;
        tb->text[0] = '\0';
        tb->active = 1;
        return tb;
    }
    return NULL;  /* Out of slots */
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Window Management
 * ═══════════════════════════════════════════════════════════════════════════ */

void forge_gui_init_window(int64_t width, int64_t height, forge_str_t title) {
    char buf[256];
    const char* t = str_cstr(title, buf, sizeof(buf));
    InitWindow((int)width, (int)height, t);
    str_cstr_free(t, title, buf);

    if (!IsWindowReady()) {
        fprintf(stderr, "FORGE: Cannot open GUI window (no display available)\n");
        exit(1);
    }
}

void forge_gui_close_window(void) {
    /* Reset textbox state */
    g_textbox_count = 0;
    CloseWindow();
}

int forge_gui_window_open(void) {
    return !WindowShouldClose();
}

void forge_gui_set_target_fps(int64_t fps) {
    SetTargetFPS((int)fps);
}

int64_t forge_gui_get_fps(void) {
    return (int64_t)GetFPS();
}

double forge_gui_get_dt(void) {
    return (double)GetFrameTime();
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Drawing Control
 * ═══════════════════════════════════════════════════════════════════════════ */

void forge_gui_begin_draw(void) {
    BeginDrawing();
}

void forge_gui_end_draw(void) {
    EndDrawing();
}

void forge_gui_clear(int64_t r, int64_t g, int64_t b, int64_t a) {
    ClearBackground(rgba(r, g, b, a));
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Shape Drawing
 * ═══════════════════════════════════════════════════════════════════════════ */

void forge_gui_draw_line(int64_t x1, int64_t y1, int64_t x2, int64_t y2,
                         int64_t r, int64_t g, int64_t b, int64_t a) {
    DrawLine((int)x1, (int)y1, (int)x2, (int)y2, rgba(r, g, b, a));
}

void forge_gui_draw_rect(int64_t x, int64_t y, int64_t w, int64_t h,
                         int64_t r, int64_t g, int64_t b, int64_t a) {
    DrawRectangle((int)x, (int)y, (int)w, (int)h, rgba(r, g, b, a));
}

void forge_gui_draw_rect_lines(int64_t x, int64_t y, int64_t w, int64_t h,
                               int64_t r, int64_t g, int64_t b, int64_t a) {
    DrawRectangleLines((int)x, (int)y, (int)w, (int)h, rgba(r, g, b, a));
}

void forge_gui_draw_circle(int64_t cx, int64_t cy, double radius,
                           int64_t r, int64_t g, int64_t b, int64_t a) {
    DrawCircle((int)cx, (int)cy, (float)radius, rgba(r, g, b, a));
}

void forge_gui_draw_circle_lines(int64_t cx, int64_t cy, double radius,
                                 int64_t r, int64_t g, int64_t b, int64_t a) {
    DrawCircleLines((int)cx, (int)cy, (float)radius, rgba(r, g, b, a));
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Text
 * ═══════════════════════════════════════════════════════════════════════════ */

void forge_gui_draw_text(forge_str_t text, int64_t x, int64_t y,
                         int64_t size, int64_t r, int64_t g, int64_t b, int64_t a) {
    char buf[1024];
    const char* t = str_cstr(text, buf, sizeof(buf));
    DrawText(t, (int)x, (int)y, (int)size, rgba(r, g, b, a));
    str_cstr_free(t, text, buf);
}

int64_t forge_gui_measure_text(forge_str_t text, int64_t size) {
    char buf[1024];
    const char* t = str_cstr(text, buf, sizeof(buf));
    int result = MeasureText(t, (int)size);
    str_cstr_free(t, text, buf);
    return (int64_t)result;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Input — Keyboard
 * ═══════════════════════════════════════════════════════════════════════════ */

int forge_gui_is_key_pressed(int64_t key) {
    return IsKeyPressed((int)key) ? 1 : 0;
}

int forge_gui_is_key_down(int64_t key) {
    return IsKeyDown((int)key) ? 1 : 0;
}

int forge_gui_is_key_released(int64_t key) {
    return IsKeyReleased((int)key) ? 1 : 0;
}

int64_t forge_gui_get_key_pressed(void) {
    return (int64_t)GetKeyPressed();
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Input — Mouse
 * ═══════════════════════════════════════════════════════════════════════════ */

int64_t forge_gui_mouse_x(void) {
    return (int64_t)GetMouseX();
}

int64_t forge_gui_mouse_y(void) {
    return (int64_t)GetMouseY();
}

int forge_gui_is_mouse_pressed(int64_t button) {
    return IsMouseButtonPressed((int)button) ? 1 : 0;
}

int forge_gui_is_mouse_down(int64_t button) {
    return IsMouseButtonDown((int)button) ? 1 : 0;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Widgets (raygui)
 * ═══════════════════════════════════════════════════════════════════════════ */

int forge_gui_button(int64_t x, int64_t y, int64_t w, int64_t h, forge_str_t text) {
    char buf[256];
    const char* t = str_cstr(text, buf, sizeof(buf));
    int result = GuiButton((Rectangle){(float)x, (float)y, (float)w, (float)h}, t);
    str_cstr_free(t, text, buf);
    return result;
}

void forge_gui_label(int64_t x, int64_t y, int64_t w, int64_t h, forge_str_t text) {
    char buf[256];
    const char* t = str_cstr(text, buf, sizeof(buf));
    GuiLabel((Rectangle){(float)x, (float)y, (float)w, (float)h}, t);
    str_cstr_free(t, text, buf);
}

int forge_gui_checkbox(int64_t x, int64_t y, int64_t w, int64_t h,
                       forge_str_t text, int checked) {
    char buf[256];
    const char* t = str_cstr(text, buf, sizeof(buf));
    bool state = checked ? true : false;
    GuiCheckBox((Rectangle){(float)x, (float)y, (float)w, (float)h}, t, &state);
    str_cstr_free(t, text, buf);
    return state ? 1 : 0;
}

double forge_gui_slider(int64_t x, int64_t y, int64_t w, int64_t h,
                        double min_val, double max_val, double value) {
    float fval = (float)value;
    GuiSlider((Rectangle){(float)x, (float)y, (float)w, (float)h},
              NULL, NULL, &fval, (float)min_val, (float)max_val);
    return (double)fval;
}

forge_str_t forge_gui_textbox(int64_t x, int64_t y, int64_t w, int64_t h,
                              forge_str_t text, int64_t max_len) {
    forge_textbox_state_t* tb = get_textbox_state(x, y);
    if (!tb) {
        /* Out of textbox slots — return input unchanged */
        return forge_str_dup(text.data, text.len);
    }

    /* Sync FORGE text into the buffer if it changed externally */
    int copy_len = text.len < TEXTBOX_BUF_SIZE - 1 ? text.len : TEXTBOX_BUF_SIZE - 1;
    if (memcmp(tb->text, text.data, copy_len) != 0 || tb->text[copy_len] != '\0') {
        memcpy(tb->text, text.data, copy_len);
        tb->text[copy_len] = '\0';
    }

    int text_size = max_len < TEXTBOX_BUF_SIZE ? (int)max_len : TEXTBOX_BUF_SIZE - 1;
    if (GuiTextBox((Rectangle){(float)x, (float)y, (float)w, (float)h},
                   tb->text, text_size, tb->edit_mode)) {
        tb->edit_mode = !tb->edit_mode;
    }

    return forge_str_dup(tb->text, (int)strlen(tb->text));
}

void forge_gui_set_style_dark(void) {
    GuiLoadStyleDefault();
    /* Apply dark theme overrides */
    GuiSetStyle(DEFAULT, BACKGROUND_COLOR, 0x2d2d2dff);
    GuiSetStyle(DEFAULT, TEXT_COLOR_NORMAL, 0xc8c8c8ff);
    GuiSetStyle(DEFAULT, BASE_COLOR_NORMAL, 0x3c3c3cff);
    GuiSetStyle(DEFAULT, BORDER_COLOR_NORMAL, 0x5a5a5aff);
    GuiSetStyle(DEFAULT, TEXT_COLOR_FOCUSED, 0xffffffff);
    GuiSetStyle(DEFAULT, BASE_COLOR_FOCUSED, 0x4a6e8aff);
    GuiSetStyle(DEFAULT, BORDER_COLOR_FOCUSED, 0x6a9ec8ff);
    GuiSetStyle(DEFAULT, TEXT_COLOR_PRESSED, 0xffffffff);
    GuiSetStyle(DEFAULT, BASE_COLOR_PRESSED, 0x3a5e7aff);
    GuiSetStyle(DEFAULT, BORDER_COLOR_PRESSED, 0x5a8eb8ff);
}

void forge_gui_set_style_light(void) {
    GuiLoadStyleDefault();
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Scrollable Text Log (ring buffer)
 * ═══════════════════════════════════════════════════════════════════════════ */

#define MAX_LOG_PANELS  8
#define LOG_LINE_MAXLEN 512
#define LOG_MAX_LINES   4096

typedef struct {
    char text[LOG_LINE_MAXLEN];
    Color color;
} forge_log_line_t;

typedef struct {
    int64_t x, y, w, h;
    int64_t font_size;
    forge_log_line_t lines[LOG_MAX_LINES];
    int line_count;         /* Total lines added */
    int head;               /* Ring buffer write position */
    int scroll_offset;      /* Lines scrolled from bottom (0 = at bottom) */
    int active;
} forge_log_panel_t;

static forge_log_panel_t g_log_panels[MAX_LOG_PANELS];

void forge_gui_log_create(int64_t id, int64_t x, int64_t y, int64_t w, int64_t h,
                          int64_t max_lines, int64_t font_size) {
    (void)max_lines;  /* We use fixed LOG_MAX_LINES internally */
    if (id < 0 || id >= MAX_LOG_PANELS) return;
    forge_log_panel_t* lp = &g_log_panels[id];
    lp->x = x;
    lp->y = y;
    lp->w = w;
    lp->h = h;
    lp->font_size = font_size > 0 ? font_size : 16;
    lp->line_count = 0;
    lp->head = 0;
    lp->scroll_offset = 0;
    lp->active = 1;
}

void forge_gui_log_add(int64_t id, forge_str_t text,
                       int64_t r, int64_t g, int64_t b, int64_t a) {
    if (id < 0 || id >= MAX_LOG_PANELS) return;
    forge_log_panel_t* lp = &g_log_panels[id];
    if (!lp->active) return;

    forge_log_line_t* line = &lp->lines[lp->head];
    int copy_len = text.len < LOG_LINE_MAXLEN - 1 ? text.len : LOG_LINE_MAXLEN - 1;
    memcpy(line->text, text.data, copy_len);
    line->text[copy_len] = '\0';
    line->color = rgba(r, g, b, a);

    lp->head = (lp->head + 1) % LOG_MAX_LINES;
    if (lp->line_count < LOG_MAX_LINES) lp->line_count++;

    /* Auto-scroll to bottom when new data arrives (if user is at bottom) */
    if (lp->scroll_offset <= 1) lp->scroll_offset = 0;
}

void forge_gui_log_clear(int64_t id) {
    if (id < 0 || id >= MAX_LOG_PANELS) return;
    forge_log_panel_t* lp = &g_log_panels[id];
    lp->line_count = 0;
    lp->head = 0;
    lp->scroll_offset = 0;
}

void forge_gui_log_draw(int64_t id) {
    if (id < 0 || id >= MAX_LOG_PANELS) return;
    forge_log_panel_t* lp = &g_log_panels[id];
    if (!lp->active || lp->line_count == 0) {
        /* Draw empty panel background */
        DrawRectangle((int)lp->x, (int)lp->y, (int)lp->w, (int)lp->h,
                      (Color){20, 20, 30, 255});
        DrawRectangleLines((int)lp->x, (int)lp->y, (int)lp->w, (int)lp->h,
                           (Color){60, 60, 80, 255});
        return;
    }

    int fs = (int)lp->font_size;
    int line_height = fs + 4;
    int visible_lines = (int)lp->h / line_height;
    if (visible_lines < 1) visible_lines = 1;

    /* Handle mouse wheel scrolling when mouse is inside the panel */
    Vector2 mouse = GetMousePosition();
    if (mouse.x >= lp->x && mouse.x <= lp->x + lp->w &&
        mouse.y >= lp->y && mouse.y <= lp->y + lp->h) {
        float wheel = GetMouseWheelMove();
        if (wheel > 0) lp->scroll_offset += 3;  /* Scroll up */
        if (wheel < 0) lp->scroll_offset -= 3;  /* Scroll down */
    }

    /* Clamp scroll offset */
    int max_scroll = lp->line_count - visible_lines;
    if (max_scroll < 0) max_scroll = 0;
    if (lp->scroll_offset < 0) lp->scroll_offset = 0;
    if (lp->scroll_offset > max_scroll) lp->scroll_offset = max_scroll;

    /* Draw panel background */
    DrawRectangle((int)lp->x, (int)lp->y, (int)lp->w, (int)lp->h,
                  (Color){20, 20, 30, 255});

    /* Enable scissor clipping to the panel area */
    BeginScissorMode((int)lp->x, (int)lp->y, (int)lp->w, (int)lp->h);

    /* Calculate the starting line index in ring buffer */
    int start_from_bottom = lp->scroll_offset;
    int lines_to_draw = visible_lines < lp->line_count ? visible_lines : lp->line_count;

    for (int i = 0; i < lines_to_draw; i++) {
        /* Line index counting from bottom of log, offset by scroll */
        int line_idx_from_end = start_from_bottom + (lines_to_draw - 1 - i);
        if (line_idx_from_end >= lp->line_count) continue;

        /* Convert to ring buffer index */
        int ring_idx = (lp->head - 1 - line_idx_from_end + LOG_MAX_LINES) % LOG_MAX_LINES;
        forge_log_line_t* line = &lp->lines[ring_idx];

        int draw_y = (int)lp->y + 2 + i * line_height;
        DrawText(line->text, (int)lp->x + 4, draw_y, fs, line->color);
    }

    EndScissorMode();

    /* Draw border */
    DrawRectangleLines((int)lp->x, (int)lp->y, (int)lp->w, (int)lp->h,
                       (Color){60, 60, 80, 255});

    /* Draw scrollbar indicator if scrollable */
    if (max_scroll > 0) {
        int sb_x = (int)(lp->x + lp->w - 8);
        int sb_h = (int)lp->h - 4;
        float thumb_ratio = (float)visible_lines / (float)lp->line_count;
        int thumb_h = (int)(sb_h * thumb_ratio);
        if (thumb_h < 10) thumb_h = 10;
        float pos_ratio = (float)(max_scroll - lp->scroll_offset) / (float)max_scroll;
        int thumb_y = (int)lp->y + 2 + (int)((sb_h - thumb_h) * pos_ratio);

        DrawRectangle(sb_x, (int)lp->y + 2, 6, sb_h, (Color){40, 40, 50, 255});
        DrawRectangle(sb_x, thumb_y, 6, thumb_h, (Color){100, 100, 130, 255});
    }
}

int64_t forge_gui_log_count(int64_t id) {
    if (id < 0 || id >= MAX_LOG_PANELS) return 0;
    return (int64_t)g_log_panels[id].line_count;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Dropdown Selector
 * ═══════════════════════════════════════════════════════════════════════════ */

#define MAX_DROPDOWNS 16

typedef struct {
    int64_t x, y;
    bool edit_mode;
    int active;
} forge_dropdown_state_t;

static forge_dropdown_state_t g_dropdown_states[MAX_DROPDOWNS];
static int g_dropdown_count = 0;

static forge_dropdown_state_t* get_dropdown_state(int64_t x, int64_t y) {
    for (int i = 0; i < g_dropdown_count; i++) {
        if (g_dropdown_states[i].x == x && g_dropdown_states[i].y == y)
            return &g_dropdown_states[i];
    }
    if (g_dropdown_count < MAX_DROPDOWNS) {
        forge_dropdown_state_t* dd = &g_dropdown_states[g_dropdown_count++];
        dd->x = x;
        dd->y = y;
        dd->edit_mode = false;
        dd->active = 1;
        return dd;
    }
    return NULL;
}

int64_t forge_gui_dropdown(int64_t x, int64_t y, int64_t w, int64_t h,
                           forge_str_t items, int64_t selected) {
    forge_dropdown_state_t* dd = get_dropdown_state(x, y);
    if (!dd) return selected;

    char buf[1024];
    const char* t = str_cstr(items, buf, sizeof(buf));
    int sel = (int)selected;

    if (GuiDropdownBox((Rectangle){(float)x, (float)y, (float)w, (float)h},
                       t, &sel, dd->edit_mode)) {
        dd->edit_mode = !dd->edit_mode;
    }

    str_cstr_free(t, items, buf);
    return (int64_t)sel;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Color Button
 * ═══════════════════════════════════════════════════════════════════════════ */

int forge_gui_color_button(int64_t x, int64_t y, int64_t w, int64_t h,
                           forge_str_t text,
                           int64_t bg_r, int64_t bg_g, int64_t bg_b, int64_t bg_a,
                           int64_t tx_r, int64_t tx_g, int64_t tx_b, int64_t tx_a) {
    Color bg = rgba(bg_r, bg_g, bg_b, bg_a);
    Color tx = rgba(tx_r, tx_g, tx_b, tx_a);

    Rectangle rect = {(float)x, (float)y, (float)w, (float)h};
    Vector2 mouse = GetMousePosition();
    int hovered = CheckCollisionPointRec(mouse, rect);
    int clicked = hovered && IsMouseButtonPressed(MOUSE_BUTTON_LEFT);

    /* Draw button background with hover effect */
    Color draw_bg = bg;
    if (hovered) {
        draw_bg.r = (unsigned char)(bg.r < 230 ? bg.r + 25 : 255);
        draw_bg.g = (unsigned char)(bg.g < 230 ? bg.g + 25 : 255);
        draw_bg.b = (unsigned char)(bg.b < 230 ? bg.b + 25 : 255);
    }
    DrawRectangleRec(rect, draw_bg);
    DrawRectangleLinesEx(rect, 1, (Color){draw_bg.r/2, draw_bg.g/2, draw_bg.b/2, 255});

    /* Draw centered text */
    char buf[256];
    const char* t = str_cstr(text, buf, sizeof(buf));
    int tw = MeasureText(t, 16);
    int tx_x = (int)x + ((int)w - tw) / 2;
    int tx_y = (int)y + ((int)h - 16) / 2;
    DrawText(t, tx_x, tx_y, 16, tx);
    str_cstr_free(t, text, buf);

    return clicked ? 1 : 0;
}

#else /* !FORGE_HAS_GUI */

/* ═══════════════════════════════════════════════════════════════════════════
 * Stub implementations — GUI not compiled in
 * ═══════════════════════════════════════════════════════════════════════════ */

static void gui_not_available(void) {
    fprintf(stderr, "FORGE: GUI support not compiled. Rebuild with: make GUI=1\n");
    exit(1);
}

void forge_gui_init_window(int64_t w, int64_t h, forge_str_t t) { (void)w; (void)h; (void)t; gui_not_available(); }
void forge_gui_close_window(void) { gui_not_available(); }
int forge_gui_window_open(void) { gui_not_available(); return 0; }
void forge_gui_set_target_fps(int64_t fps) { (void)fps; gui_not_available(); }
int64_t forge_gui_get_fps(void) { gui_not_available(); return 0; }
double forge_gui_get_dt(void) { gui_not_available(); return 0.0; }

void forge_gui_begin_draw(void) { gui_not_available(); }
void forge_gui_end_draw(void) { gui_not_available(); }
void forge_gui_clear(int64_t r, int64_t g, int64_t b, int64_t a) { (void)r; (void)g; (void)b; (void)a; gui_not_available(); }

void forge_gui_draw_line(int64_t x1,int64_t y1,int64_t x2,int64_t y2,int64_t r,int64_t g,int64_t b,int64_t a) { (void)x1;(void)y1;(void)x2;(void)y2;(void)r;(void)g;(void)b;(void)a; gui_not_available(); }
void forge_gui_draw_rect(int64_t x,int64_t y,int64_t w,int64_t h,int64_t r,int64_t g,int64_t b,int64_t a) { (void)x;(void)y;(void)w;(void)h;(void)r;(void)g;(void)b;(void)a; gui_not_available(); }
void forge_gui_draw_rect_lines(int64_t x,int64_t y,int64_t w,int64_t h,int64_t r,int64_t g,int64_t b,int64_t a) { (void)x;(void)y;(void)w;(void)h;(void)r;(void)g;(void)b;(void)a; gui_not_available(); }
void forge_gui_draw_circle(int64_t cx,int64_t cy,double radius,int64_t r,int64_t g,int64_t b,int64_t a) { (void)cx;(void)cy;(void)radius;(void)r;(void)g;(void)b;(void)a; gui_not_available(); }
void forge_gui_draw_circle_lines(int64_t cx,int64_t cy,double radius,int64_t r,int64_t g,int64_t b,int64_t a) { (void)cx;(void)cy;(void)radius;(void)r;(void)g;(void)b;(void)a; gui_not_available(); }

void forge_gui_draw_text(forge_str_t text,int64_t x,int64_t y,int64_t size,int64_t r,int64_t g,int64_t b,int64_t a) { (void)text;(void)x;(void)y;(void)size;(void)r;(void)g;(void)b;(void)a; gui_not_available(); }
int64_t forge_gui_measure_text(forge_str_t text,int64_t size) { (void)text;(void)size; gui_not_available(); return 0; }

int forge_gui_is_key_pressed(int64_t key) { (void)key; gui_not_available(); return 0; }
int forge_gui_is_key_down(int64_t key) { (void)key; gui_not_available(); return 0; }
int forge_gui_is_key_released(int64_t key) { (void)key; gui_not_available(); return 0; }
int64_t forge_gui_get_key_pressed(void) { gui_not_available(); return 0; }

int64_t forge_gui_mouse_x(void) { gui_not_available(); return 0; }
int64_t forge_gui_mouse_y(void) { gui_not_available(); return 0; }
int forge_gui_is_mouse_pressed(int64_t button) { (void)button; gui_not_available(); return 0; }
int forge_gui_is_mouse_down(int64_t button) { (void)button; gui_not_available(); return 0; }

int forge_gui_button(int64_t x,int64_t y,int64_t w,int64_t h,forge_str_t text) { (void)x;(void)y;(void)w;(void)h;(void)text; gui_not_available(); return 0; }
void forge_gui_label(int64_t x,int64_t y,int64_t w,int64_t h,forge_str_t text) { (void)x;(void)y;(void)w;(void)h;(void)text; gui_not_available(); }
int forge_gui_checkbox(int64_t x,int64_t y,int64_t w,int64_t h,forge_str_t text,int checked) { (void)x;(void)y;(void)w;(void)h;(void)text;(void)checked; gui_not_available(); return 0; }
double forge_gui_slider(int64_t x,int64_t y,int64_t w,int64_t h,double mn,double mx,double v) { (void)x;(void)y;(void)w;(void)h;(void)mn;(void)mx;(void)v; gui_not_available(); return 0.0; }
forge_str_t forge_gui_textbox(int64_t x,int64_t y,int64_t w,int64_t h,forge_str_t text,int64_t ml) { (void)x;(void)y;(void)w;(void)h;(void)text;(void)ml; gui_not_available(); return forge_str_lit(""); }

void forge_gui_set_style_dark(void) { gui_not_available(); }
void forge_gui_set_style_light(void) { gui_not_available(); }

/* New widget stubs */
void forge_gui_log_create(int64_t id,int64_t x,int64_t y,int64_t w,int64_t h,int64_t ml,int64_t fs) { (void)id;(void)x;(void)y;(void)w;(void)h;(void)ml;(void)fs; gui_not_available(); }
void forge_gui_log_add(int64_t id,forge_str_t text,int64_t r,int64_t g,int64_t b,int64_t a) { (void)id;(void)text;(void)r;(void)g;(void)b;(void)a; gui_not_available(); }
void forge_gui_log_clear(int64_t id) { (void)id; gui_not_available(); }
void forge_gui_log_draw(int64_t id) { (void)id; gui_not_available(); }
int64_t forge_gui_log_count(int64_t id) { (void)id; gui_not_available(); return 0; }
int64_t forge_gui_dropdown(int64_t x,int64_t y,int64_t w,int64_t h,forge_str_t items,int64_t sel) { (void)x;(void)y;(void)w;(void)h;(void)items;(void)sel; gui_not_available(); return 0; }
int forge_gui_color_button(int64_t x,int64_t y,int64_t w,int64_t h,forge_str_t text,int64_t br,int64_t bg,int64_t bb,int64_t ba,int64_t tr,int64_t tg,int64_t tb,int64_t ta) { (void)x;(void)y;(void)w;(void)h;(void)text;(void)br;(void)bg;(void)bb;(void)ba;(void)tr;(void)tg;(void)tb;(void)ta; gui_not_available(); return 0; }

#endif /* FORGE_HAS_GUI */

