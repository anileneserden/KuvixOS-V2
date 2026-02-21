// kernel/ui/apps/terminal.c

#include <app/app.h>
#include <ui/wm.h>
#include <kernel/drivers/video/gfx.h>
#include <lib/string.h>
#include <stdint.h>
#include <stdbool.h>
#include <kernel/user.h>

#include <lib/commands.h>   // commands_execute + output/clear hook

// dış
extern char kbd_scancode_to_ascii(uint8_t scancode);

typedef struct {
    char line[256];
    int  len;

    char out[4096];
    int  out_len;

    char cwd[128];
    user_lang_t lang;
} terminal_t;

static terminal_t* term(app_t* app) {
    return (app && app->user) ? (terminal_t*)app->user : NULL;
}

static void term_out(terminal_t* t, const char* s) {
    if (!t || !s) return;

    int sl = (int)strlen(s);
    if (sl <= 0) return;

    if (t->out_len + sl >= (int)sizeof(t->out) - 1) {
        t->out_len = 0;
        t->out[0] = '\0';
    }

    memcpy(t->out + t->out_len, s, (size_t)sl);
    t->out_len += sl;
    t->out[t->out_len] = '\0';
}

// prompt string üret (BURADA $ YOK! user_format_prompt sadece user@host:path üretmeli)
static void terminal_get_prompt(terminal_t* t, char* out, int out_sz) {
    if (!t || !out || out_sz <= 0) return;
    user_format_prompt(t->cwd, out, out_sz, t->lang);
}

// prompt’u history/out buffer’a bas (enter/clear sonrası için)
static void terminal_prompt_out(terminal_t* t) {
    char p[256];
    terminal_get_prompt(t, p, sizeof(p));
    term_out(t, p);
}

// reset screen
static void terminal_reset_screen(terminal_t* t) {
    if (!t) return;

    t->out_len = 0;
    t->out[0] = '\0';
    t->len = 0;
    t->line[0] = '\0';

    term_out(t, "KuvixOS Terminal v0.2\n");
    term_out(t, "help yaz.\n");
    terminal_prompt_out(t);
}

// ------------------------------------------------------------
// commands output callback -> terminal buffer
// ------------------------------------------------------------
static void terminal_cmd_write(void* user, const char* s) {
    terminal_t* t = (terminal_t*)user;
    term_out(t, s ? s : "");
}

// commands clear callback -> terminal buffer temizle
static void terminal_cmd_clear(void* user) {
    terminal_t* t = (terminal_t*)user;
    if (!t) return;

    t->out_len = 0;
    t->out[0] = '\0';
    t->len = 0;
    t->line[0] = '\0';
}

// ------------------------------------------------------------
// helpers: draw text and advance cursor (8px grid)
// ------------------------------------------------------------
static void draw_char_advance(char c, uint32_t col, int* x, int* y, int max_w) {
    char s[2] = {c, 0};
    gfx_draw_text_utf8(*x, *y, col, s);
    *x += 8;
    if (*x > max_w - 16) { *y += 14; *x = 8; }
}

// ------------------------------------------------------------
// vtbl callbackler
// ------------------------------------------------------------
static void terminal_on_create(app_t* app) {
    terminal_t* t = term(app);
    if (!t) return;

    t->len = 0;
    t->line[0] = '\0';
    t->out_len = 0;
    t->out[0] = '\0';

    strncpy(t->cwd, USER_DESKTOP_PATH, sizeof(t->cwd)-1);
    t->cwd[sizeof(t->cwd)-1] = '\0';
    t->lang = USER_LANG_EN;

    terminal_reset_screen(t);

    commands_set_output(terminal_cmd_write, t);
    commands_set_clear(terminal_cmd_clear, t);
}

static void terminal_on_draw(app_t* app) {
    terminal_t* t = term(app);
    if (!t) return;

    ui_rect_t client = wm_get_client_rect(app->win_id);
    gfx_fill_rect(0, 0, client.w, client.h, 0x000000);

    int x = 8;
    int y = 8;

    const int line_h = 14;
    const int margin_x = 8;
    const int bottom_limit = client.h - 20;

    // 1) out buffer çiz
    for (int i = 0; i < t->out_len; i++) {
        char c = t->out[i];

        if (c == '\n') {
            y += line_h;
            x = margin_x;
        } else if (c == '\r') {
            // CR: satır başına dön
            x = margin_x;
        } else if (c == '\t') {
            // TAB: 4 boşluk gibi
            for (int k = 0; k < 4; k++) {
                draw_char_advance(' ', 0x00FF00, &x, &y, client.w);
                if (y > bottom_limit) break;
            }
        } else {
            draw_char_advance(c, 0x00FF00, &x, &y, client.w);
        }

        if (y > bottom_limit) break;
    }

    // 2) input çiz (t->line)
    for (int i = 0; i < t->len; i++) {
        draw_char_advance(t->line[i], 0x00FF00, &x, &y, client.w);
        if (y > bottom_limit) break;
    }

    // 3) cursor: satır sonundaysa wrap’le
    if (x > client.w - 16) { y += line_h; x = margin_x; }
    if (y <= bottom_limit) gfx_draw_text_utf8(x, y, 0x00FF00, "_");
}

static void terminal_on_mouse(app_t* app, int mx, int my, uint8_t pressed, uint8_t released, uint8_t buttons) {
    (void)app; (void)mx; (void)my; (void)pressed; (void)released; (void)buttons;
}

static void terminal_on_key(app_t* app, uint16_t key) {
    terminal_t* t = term(app);
    if (!app || !t) return;

    uint8_t sc = (uint8_t)(key & 0xFF);
    if (sc & 0x80) return;

    // Enter
    if (sc == 0x1C) {
        commands_set_output(terminal_cmd_write, t);
        commands_set_clear(terminal_cmd_clear, t);
        commands_set_cwd(t->cwd);

        t->line[t->len] = '\0';

        // kullanıcının komutunu history'e geçir:
        // (prompt + $ zaten out'ta var; burada sadece line + newline yeterli)
        term_out(t, t->line);
        term_out(t, "\n");


        // çalıştır
        commands_execute(t->line);

        // yeni prompt
        terminal_prompt_out(t);
        term_out(t, "$ ");

        // input temizle
        t->len = 0;
        t->line[0] = '\0';
        return;
    }

    char c = kbd_scancode_to_ascii(sc);

    // Backspace
    if (c == '\b') {
        if (t->len > 0) {
            t->len--;
            t->line[t->len] = '\0';
        }
        return;
    }

    // Printable
    if (c >= 32 && c <= 126) {
        if (t->len < (int)sizeof(t->line) - 1) {
            t->line[t->len++] = c;
            t->line[t->len] = '\0';
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