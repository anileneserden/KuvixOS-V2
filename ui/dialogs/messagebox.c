// ui/dialogs/messagebox.c

#include <ui/dialogs/messagebox.h>
#include <kernel/drivers/video/gfx.h>
#include <kernel/drivers/video/fb.h>
#include <lib/string.h>
#include <stdbool.h>
#include <stdint.h>

// redraw / damage
extern void desktop_damage_rect(int x, int y, int w, int h);
extern void desktop_request_redraw(void);

// ------------------------------------------------------------
// Layout constants
// ------------------------------------------------------------
#define MB_MIN_W        320
#define MB_MAX_W        560
#define MB_MIN_H        150

#define MB_PAD_X        20
#define MB_PAD_Y        18
#define MB_TITLE_H      25

#define MB_BTN_W        80
#define MB_BTN_H        25
#define MB_BTN_GAP      20
#define MB_BTN_AREA_H   48

#define MB_FONT_W       8
#define MB_FONT_H       16

#define MB_MAX_LINES    12
#define MB_MAX_COLS     96

// ------------------------------------------------------------
// Private state
// ------------------------------------------------------------
static char _title[64];
static char _text[256];

static bool _visible = false;
static MB_BTNS_T _active_btns;
static MB_RES_T _result = MB_RES_NONE;

// Pencere boyut/konum
static int _win_w = MB_MIN_W;
static int _win_h = MB_MIN_H;
static int _win_x = 0;
static int _win_y = 0;

// wrapped text
static char _lines[MB_MAX_LINES][MB_MAX_COLS];
static int  _line_count = 0;

// ------------------------------------------------------------
// Helpers
// ------------------------------------------------------------
static void messagebox_request_redraw(void) {
    if (_visible) {
        desktop_damage_rect(_win_x, _win_y, _win_w, _win_h);
    }
    desktop_request_redraw();
}

static void messagebox_close_internal(void) {
    if (_visible) {
        desktop_damage_rect(_win_x, _win_y, _win_w, _win_h);
    }
    _visible = false;
    desktop_request_redraw();
}

static int max_i(int a, int b) { return (a > b) ? a : b; }
static int min_i(int a, int b) { return (a < b) ? a : b; }

static void clear_lines(void) {
    memset(_lines, 0, sizeof(_lines));
    _line_count = 0;
}

static int line_len_chars(const char* s) {
    return s ? (int)strlen(s) : 0;
}

static int longest_line_chars(void) {
    int maxlen = 0;
    for (int i = 0; i < _line_count; i++) {
        int len = line_len_chars(_lines[i]);
        if (len > maxlen) maxlen = len;
    }
    return maxlen;
}

// Basit word-wrap:
// - boşlukta kırmayı dener
// - newline destekler
// - gerekirse zorla böler
static void wrap_text_to_lines(const char* text, int max_chars_per_line) {
    clear_lines();

    if (!text || !text[0]) {
        _line_count = 1;
        _lines[0][0] = '\0';
        return;
    }

    if (max_chars_per_line < 8) max_chars_per_line = 8;
    if (max_chars_per_line >= MB_MAX_COLS) max_chars_per_line = MB_MAX_COLS - 1;

    int src = 0;
    int li = 0;

    while (text[src] && li < MB_MAX_LINES) {
        while (text[src] == ' ') src++;

        if (!text[src]) break;

        int start = src;
        int count = 0;
        int last_space = -1;

        while (text[src] && text[src] != '\n' && count < max_chars_per_line) {
            if (text[src] == ' ') last_space = src;
            src++;
            count++;
        }

        if (text[src] == '\n') {
            int len = src - start;
            if (len < 0) len = 0;
            if (len >= MB_MAX_COLS) len = MB_MAX_COLS - 1;

            memcpy(_lines[li], &text[start], (size_t)len);
            _lines[li][len] = '\0';
            li++;
            src++; // '\n' geç
            continue;
        }

        if (count >= max_chars_per_line && text[src] && text[src] != '\n') {
            // satır dolduysa boşlukta kır
            int end = src;

            if (last_space >= start) {
                end = last_space;
                src = last_space + 1;
            }

            int len = end - start;
            if (len < 0) len = 0;
            if (len >= MB_MAX_COLS) len = MB_MAX_COLS - 1;

            memcpy(_lines[li], &text[start], (size_t)len);
            _lines[li][len] = '\0';
            li++;
            continue;
        }

        // normal satır sonu / metin sonu
        {
            int end = src;
            int len = end - start;
            if (len < 0) len = 0;
            if (len >= MB_MAX_COLS) len = MB_MAX_COLS - 1;

            memcpy(_lines[li], &text[start], (size_t)len);
            _lines[li][len] = '\0';
            li++;
        }
    }

    if (li <= 0) {
        li = 1;
        _lines[0][0] = '\0';
    }

    _line_count = li;
}

static void compute_window_layout(void) {
    // önce kabaca wrap
    int max_chars = 42;
    wrap_text_to_lines(_text, max_chars);

    // en uzun satıra göre width
    int longest = longest_line_chars();
    int text_w = longest * MB_FONT_W;
    int desired_w = text_w + MB_PAD_X * 2;

    // buton alanı da minimum genişlik gerektirir
    int buttons_min_w = 0;
    if (_active_btns == MB_BTNS_OK) {
        buttons_min_w = MB_BTN_W + MB_PAD_X * 2;
    } else {
        buttons_min_w = (MB_BTN_W * 2) + MB_BTN_GAP + MB_PAD_X * 2;
    }

    desired_w = max_i(desired_w, buttons_min_w);
    desired_w = max_i(desired_w, MB_MIN_W);
    desired_w = min_i(desired_w, MB_MAX_W);

    _win_w = desired_w;

    // şimdi gerçek width'e göre tekrar wrap
    max_chars = (_win_w - MB_PAD_X * 2) / MB_FONT_W;
    if (max_chars < 8) max_chars = 8;
    wrap_text_to_lines(_text, max_chars);

    int text_h = _line_count * MB_FONT_H;
    int desired_h = MB_TITLE_H + MB_PAD_Y + text_h + MB_BTN_AREA_H + MB_PAD_Y;
    if (desired_h < MB_MIN_H) desired_h = MB_MIN_H;

    _win_h = desired_h;

    // ekran ortala
    int sw = (int)fb_get_width();
    int sh = (int)fb_get_height();
    _win_x = (sw - _win_w) / 2;
    _win_y = (sh - _win_h) / 2;
}

static void calc_ok_button_rect(int* x, int* y, int* w, int* h) {
    int bx = _win_x + (_win_w - MB_BTN_W) / 2;
    int by = _win_y + _win_h - MB_BTN_AREA_H + 10;

    if (x) *x = bx;
    if (y) *y = by;
    if (w) *w = MB_BTN_W;
    if (h) *h = MB_BTN_H;
}

static void calc_yes_button_rect(int* x, int* y, int* w, int* h) {
    int total_w = (MB_BTN_W * 2) + MB_BTN_GAP;
    int start_x = _win_x + (_win_w - total_w) / 2;
    int by = _win_y + _win_h - MB_BTN_AREA_H + 10;

    if (x) *x = start_x;
    if (y) *y = by;
    if (w) *w = MB_BTN_W;
    if (h) *h = MB_BTN_H;
}

static void calc_no_button_rect(int* x, int* y, int* w, int* h) {
    int total_w = (MB_BTN_W * 2) + MB_BTN_GAP;
    int start_x = _win_x + (_win_w - total_w) / 2;
    int by = _win_y + _win_h - MB_BTN_AREA_H + 10;

    if (x) *x = start_x + MB_BTN_W + MB_BTN_GAP;
    if (y) *y = by;
    if (w) *w = MB_BTN_W;
    if (h) *h = MB_BTN_H;
}

static bool hit_rect(int mx, int my, int x, int y, int w, int h) {
    return (mx >= x && mx < x + w && my >= y && my < y + h);
}

static void draw_button(int x, int y, int w, int h, const char* text) {
    gfx_fill_rect(x, y, w, h, 0xAAAAAA);
    gfx_draw_rect(x, y, w, h, 0x000000);

    int len = (int)strlen(text);
    int tw = len * MB_FONT_W;
    int tx = x + (w - tw) / 2;
    int ty = y + (h - MB_FONT_H) / 2;

    gfx_draw_text(tx, ty, 0x000000, text);
}

// ------------------------------------------------------------
// Private API
// ------------------------------------------------------------
static void _show(const char* title, const char* text, MB_ICON_T icon, MB_BTNS_T buttons) {
    (void)icon; // şimdilik kullanılmıyor

    strncpy(_title, title ? title : "", sizeof(_title) - 1);
    _title[sizeof(_title) - 1] = '\0';

    strncpy(_text, text ? text : "", sizeof(_text) - 1);
    _text[sizeof(_text) - 1] = '\0';

    _active_btns = buttons;
    _result = MB_RES_NONE;
    _visible = true;

    compute_window_layout();
    messagebox_request_redraw();
}

static void _close(void) {
    messagebox_close_internal();
}

// ------------------------------------------------------------
// System functions
// ------------------------------------------------------------
void messagebox_draw(void) {
    if (!_visible) return;

    // gölge
    gfx_fill_rect(_win_x + 4, _win_y + 4, _win_w, _win_h, 0x222222);

    // gövde
    gfx_fill_rect(_win_x, _win_y, _win_w, _win_h, 0xDDDDDD);
    gfx_draw_rect(_win_x, _win_y, _win_w, _win_h, 0x000000);

    // başlık
    gfx_fill_rect(_win_x, _win_y, _win_w, MB_TITLE_H, 0x0000AA);
    gfx_draw_text(_win_x + 10, _win_y + 5, 0xFFFFFF, _title);

    // metin
    {
        int text_x = _win_x + MB_PAD_X;
        int text_y = _win_y + MB_TITLE_H + MB_PAD_Y;

        for (int i = 0; i < _line_count; i++) {
            gfx_draw_text(text_x, text_y + i * MB_FONT_H, 0x000000, _lines[i]);
        }
    }

    // butonlar
    if (_active_btns == MB_BTNS_OK) {
        int bx, by, bw, bh;
        calc_ok_button_rect(&bx, &by, &bw, &bh);
        draw_button(bx, by, bw, bh, "Tamam");
    } else if (_active_btns == MB_BTNS_YESNO) {
        int ex, ey, ew, eh;
        int hx, hy, hw, hh;

        calc_yes_button_rect(&ex, &ey, &ew, &eh);
        calc_no_button_rect(&hx, &hy, &hw, &hh);

        draw_button(ex, ey, ew, eh, "Evet");
        draw_button(hx, hy, hw, hh, "Hayir");
    }
}

void messagebox_handle_mouse(int mx, int my, bool pressed) {
    if (!_visible || !pressed) return;

    if (_active_btns == MB_BTNS_OK) {
        int bx, by, bw, bh;
        calc_ok_button_rect(&bx, &by, &bw, &bh);

        if (hit_rect(mx, my, bx, by, bw, bh)) {
            _result = MB_RES_OK;
            messagebox_close_internal();
            return;
        }
    } else if (_active_btns == MB_BTNS_YESNO) {
        int ex, ey, ew, eh;
        int hx, hy, hw, hh;

        calc_yes_button_rect(&ex, &ey, &ew, &eh);
        calc_no_button_rect(&hx, &hy, &hw, &hh);

        if (hit_rect(mx, my, ex, ey, ew, eh)) {
            _result = MB_RES_YES;
            messagebox_close_internal();
            return;
        }

        if (hit_rect(mx, my, hx, hy, hw, hh)) {
            _result = MB_RES_NO;
            messagebox_close_internal();
            return;
        }
    }
}

bool messagebox_is_visible(void) {
    return _visible;
}

MB_RES_T messagebox_get_result(void) {
    return _result;
}

void messagebox_reset_result(void) {
    _result = MB_RES_NONE;
}

// ------------------------------------------------------------
// Global namespace objects
// ------------------------------------------------------------
MessageBoxButtons_Wrapper MessageBoxButtons = {
    .OK = MB_BTNS_OK,
    .YesNo = MB_BTNS_YESNO
};

MessageBox_Namespace MessageBox = {
    .Show = _show,
    .Close = _close
};