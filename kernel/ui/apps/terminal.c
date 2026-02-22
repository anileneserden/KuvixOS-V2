// kernel/ui/apps/terminal.c

#include <ui/apps/terminal.h>

#include <app/app.h>
#include <ui/wm.h>
#include <kernel/drivers/video/gfx.h>
#include <lib/string.h>
#include <stdint.h>
#include <stdbool.h>

#include <kernel/user.h>
#include <lib/commands.h>

// dış
extern char kbd_scancode_to_ascii(uint8_t scancode);

// ------------------------------------------------------------
// helpers
// ------------------------------------------------------------
static terminal_t* term(app_t* app) {
    return (app && app->user) ? (terminal_t*)app->user : NULL;
}

static void term_recalc_layout(terminal_t* t, int client_w, int client_h) {
    if (!t) return;

    // font8x16 çiziyorsun ama satır aralığını 14 tutuyorsun
    t->line_h = 14;

    t->cols = (client_w > 0) ? (client_w / 8) : 80;
    if (t->cols < 10) t->cols = 10;

    // üst-alt margin 8 + 8 = 16 px
    int usable_h = client_h - 16;
    if (usable_h < t->line_h) usable_h = t->line_h;

    t->rows = (usable_h / t->line_h);
    if (t->rows < 1) t->rows = 1;
}

static int term_max_visible_lines(const terminal_t* t) {
    return t ? t->rows : 1;
}

static void term_scroll_to_bottom(terminal_t* t) {
    if (!t) return;
    int vis = term_max_visible_lines(t);
    if (t->line_count <= vis) t->view_start = 0;
    else t->view_start = t->line_count - vis;
}

static term_line_t* term_cur_line(terminal_t* t) {
    if (!t) return NULL;

    if (t->line_count <= 0) {
        t->line_count = 1;
        t->lines[0].len = 0;
        t->lines[0].text[0] = '\0';
        t->view_start = 0;
    }
    return &t->lines[t->line_count - 1];
}

static void term_newline(terminal_t* t) {
    if (!t) return;

    if (t->line_count < TERM_MAX_LINES) {
        t->line_count++;
    } else {
        // shift up
        for (int i = 1; i < TERM_MAX_LINES; i++) {
            t->lines[i - 1] = t->lines[i];
        }
        t->line_count = TERM_MAX_LINES;
        if (t->view_start > 0) t->view_start--;
    }

    term_line_t* ln = &t->lines[t->line_count - 1];
    ln->len = 0;
    ln->text[0] = '\0';
}

static void term_putc(terminal_t* t, char c) {
    if (!t) return;

    if (c == '\r') return;

    if (c == '\n') {
        term_newline(t);
        return;
    }

    term_line_t* ln = term_cur_line(t);
    if (!ln) return;

    // wrap if full (line buffer or screen cols)
    if (ln->len >= (TERM_LINE_MAX - 1) || ln->len >= (t->cols - 1)) {
        term_newline(t);
        ln = term_cur_line(t);
        if (!ln) return;
    }

    ln->text[ln->len++] = c;
    ln->text[ln->len] = '\0';
}

static void term_write(terminal_t* t, const char* s) {
    if (!t || !s) return;
    while (*s) term_putc(t, *s++);
}

// prompt: user@host:~/path$ 
static void term_prompt_out(terminal_t* t) {
    char p[256];
    p[0] = 0;

    // user_format_prompt: user@host:~/path
    user_format_prompt(t->cwd, p, (int)sizeof(p), t->lang);

    term_write(t, p);
    term_write(t, "$ ");
}

static void terminal_reset(terminal_t* t) {
    if (!t) return;

    t->in_len = 0;
    t->input[0] = '\0';

    t->line_count = 0;
    t->view_start = 0;

    term_newline(t);

    term_write(t, "KuvixOS Terminal v0.2\n");
    term_write(t, "help yaz.\n");
    term_prompt_out(t);

    term_scroll_to_bottom(t);
}

// ------------------------------------------------------------
// commands hooks
// ------------------------------------------------------------
static void terminal_cmd_write(void* user, const char* s) {
    terminal_t* t = (terminal_t*)user;
    if (!t) return;

    term_write(t, s ? s : "");
    term_scroll_to_bottom(t);
}

static void terminal_cmd_clear(void* user) {
    terminal_t* t = (terminal_t*)user;
    if (!t) return;

    t->in_len = 0;
    t->input[0] = '\0';

    t->line_count = 0;
    t->view_start = 0;

    term_newline(t);
    term_prompt_out(t);
    term_scroll_to_bottom(t);

    t->suppress_next_prompt = 1;
}

// ------------------------------------------------------------
// scroll
// ------------------------------------------------------------
static void terminal_scroll_lines(terminal_t* t, int delta) {
    if (!t) return;

    int vis = term_max_visible_lines(t);
    if (t->line_count <= vis) {
        t->view_start = 0;
        return;
    }

    t->view_start += delta;
    if (t->view_start < 0) t->view_start = 0;

    int max_start = t->line_count - vis;
    if (t->view_start > max_start) t->view_start = max_start;
}

// ------------------------------------------------------------
// vtbl callbacks
// ------------------------------------------------------------
static void terminal_on_create(app_t* app) {
    terminal_t* t = term(app);
    if (!t) return;

    memset(t, 0, sizeof(*t));

    strncpy(t->cwd, USER_DESKTOP_PATH, sizeof(t->cwd) - 1);
    t->cwd[sizeof(t->cwd) - 1] = '\0';
    t->lang = USER_LANG_EN;

    // defaults until first draw recalculates
    t->line_h = 14;
    t->cols   = 80;
    t->rows   = 20;

    commands_set_output(terminal_cmd_write, t);
    commands_set_clear(terminal_cmd_clear, t);

    terminal_reset(t);
}

static void terminal_on_draw(app_t* app) {
    terminal_t* t = term(app);
    if (!t) return;

    ui_rect_t client = wm_get_client_rect(app->win_id);

    gfx_fill_rect(0, 0, client.w, client.h, 0x000000);

    term_recalc_layout(t, client.w, client.h);

    const int margin_x = 8;
    const int margin_y = 8;

    // clamp view_start
    if (t->view_start < 0) t->view_start = 0;
    if (t->view_start > t->line_count) t->view_start = t->line_count;

    // draw scrollback visible lines
    int vis = term_max_visible_lines(t);

    int y = margin_y;
    for (int row = 0; row < vis; row++) {
        int li = t->view_start + row;
        if (li >= t->line_count) break;

        gfx_draw_text_utf8(margin_x, y, 0x00FF00, t->lines[li].text);
        y += t->line_h;
    }

    // kaç satır gerçekten çizildi?
    int content_rows = t->line_count - t->view_start;
    if (content_rows < 0) content_rows = 0;
    if (content_rows > vis) content_rows = vis;

    // hiç satır yoksa güvenli fallback
    if (content_rows <= 0) content_rows = 1;

    // ekranda görünen son satır indexi
    int last_index = t->view_start + content_rows - 1;
    if (last_index < 0) last_index = 0;
    if (last_index >= t->line_count) last_index = t->line_count - 1;
    if (last_index < 0) last_index = 0;

    // cursor/input'un çizileceği Y: gerçekten çizilen son satırın Y'si
    int base_y = margin_y + (content_rows - 1) * t->line_h;
    if (base_y < margin_y) base_y = margin_y;

    int base_len = 0;
    if (last_index >= 0 && last_index < t->line_count) base_len = t->lines[last_index].len;

    int x = margin_x + (base_len * 8);
    int y2 = base_y;

    // Eğer satır çok uzunsa wrap olur; basit wrap:
    int col = base_len;
    for (int i = 0; i < t->in_len; i++) {
        char s[2] = { t->input[i], 0 };
        gfx_draw_text_utf8(x, y2, 0x00FF00, s);
        x += 8;
        col++;

        if (col >= t->cols - 1) {
            col = 0;
            x = margin_x;
            y2 += t->line_h;
            if (y2 > (client.h - margin_y - t->line_h)) break;
        }
    }

    // cursor
    gfx_draw_text_utf8(x, y2, 0x00FF00, "_");
}

static void terminal_on_mouse(app_t* app, int mx, int my, uint8_t pressed, uint8_t released, uint8_t buttons) {
    (void)app; (void)mx; (void)my; (void)pressed; (void)released; (void)buttons;
    // Wheel WM route edince burada scroll çağıracağız.
}

static void terminal_on_key(app_t* app, uint16_t key) {
    terminal_t* t = term(app);
    if (!t) return;

    uint8_t sc = (uint8_t)(key & 0xFF);
    if (sc & 0x80) return; // break ignore

    // PageUp/PageDown (set1)
    if (sc == 0x49) { terminal_scroll_lines(t, -3); return; }
    if (sc == 0x51) { terminal_scroll_lines(t, +3); return; }

    // Enter
    if (sc == 0x1C) {
        commands_set_output(terminal_cmd_write, t);
        commands_set_clear(terminal_cmd_clear, t);
        commands_set_cwd(t->cwd);

        t->input[t->in_len] = '\0';

        // komutu history'e geçir
        term_write(t, t->input);
        term_write(t, "\n");

        // çalıştır
        commands_execute(t->input);

        if (!t->suppress_next_prompt) {
            term_prompt_out(t);
        }
        t->suppress_next_prompt = 0;
        term_scroll_to_bottom(t);

        // input temizle
        t->in_len = 0;
        t->input[0] = '\0';
        return;
    }

    char c = kbd_scancode_to_ascii(sc);

    // Backspace
    if (c == '\b' || (uint8_t)c == 127) {
        if (t->in_len > 0) {
            t->in_len--;
            t->input[t->in_len] = '\0';
        }
        return;
    }

    // Printable
    if (c >= 32 && c <= 126) {
        if (t->in_len < TERM_INPUT_MAX - 1) {
            t->input[t->in_len++] = c;
            t->input[t->in_len] = '\0';
        }
    }
}

static void terminal_on_destroy(app_t* app) {
    (void)app;
}

const app_vtbl_t terminal_vtbl = {
    .on_create  = terminal_on_create,
    .on_draw    = terminal_on_draw,
    .on_mouse   = terminal_on_mouse,
    .on_key     = terminal_on_key,
    .on_destroy = terminal_on_destroy
};