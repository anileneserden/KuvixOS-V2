// ui/apps/terminal.c

#include <ui/apps/terminal.h>
#include <app/app.h>
#include <ui/wm.h>
#include <kernel/drivers/video/gfx.h>
#include <lib/string.h>
#include <stdint.h>
#include <stdbool.h>
#include <kernel/user.h>
#include <lib/commands.h>

extern char kbd_scancode_to_ascii(uint8_t scancode);

static term_line_t g_term_lines[TERM_MAX_LINES];

// ------------------------------------------------------------
// Helpers
// ------------------------------------------------------------
static terminal_t* term(app_t* app) {
    return (app && app->user) ? (terminal_t*)app->user : NULL;
}

static void term_recalc_layout(terminal_t* t, int client_w, int client_h) {
    if (!t) return;
    t->line_h = 14;
    t->cols = (client_w > 0) ? (client_w / 8) : 80;
    if (t->cols < 10) t->cols = 10;
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
        g_term_lines[0].len = 0;
        g_term_lines[0].text[0] = '\0';
        t->view_start = 0;
    }
    return &g_term_lines[t->line_count - 1];
}

static void term_newline(terminal_t* t) {
    if (!t) return;
    if (t->line_count < TERM_MAX_LINES) {
        t->line_count++;
    } else {
        for (int i = 1; i < TERM_MAX_LINES; i++) {
            g_term_lines[i - 1] = g_term_lines[i];
        }
        t->line_count = TERM_MAX_LINES;
        if (t->view_start > 0) t->view_start--;
    }
    term_line_t* ln = &g_term_lines[t->line_count - 1];
    ln->len = 0;
    ln->text[0] = '\0';
}

static void term_putc(terminal_t* t, char c) {
    if (!t) return;

    if (c == '\r') {
        term_line_t* ln = term_cur_line(t);
        if (ln) {
            ln->len = 0;
            // ✅ Satırı fiziksel olarak değil, mantıksal olarak sıfırla
            // Yazılacak yeni karakterler eskisinin üzerine binecek.
        }
        return;
    }

    if (c == '\n') {
        term_newline(t);
        return;
    }

    term_line_t* ln = term_cur_line(t);
    if (!ln) return;

    if (ln->len >= (TERM_LINE_MAX - 1) || ln->len >= (t->cols - 1)) {
        term_newline(t);
        ln = term_cur_line(t);
        if (!ln) return;
    }

    // Karakteri yaz (overwrite)
    ln->text[ln->len++] = c;

    // ✅ EN ÖNEMLİ DÜZELTME:
    // Her karakterden sonra mutlaka NULL koy. 
    // Böylece eski uzun metinlerden (progress bar'ın kalıntıları gibi) 
    // eser kalmaz, terminal sadece yeni yazdığın kadarını çizer.
    ln->text[ln->len] = '\0';
}

static void term_write(terminal_t* t, const char* s) {
    if (!t || !s) return;
    while (*s) term_putc(t, *s++);
}

static void term_prompt_out(terminal_t* t) {
    char p[256]; p[0] = 0;
    user_format_prompt(t->cwd, p, (int)sizeof(p), t->lang);
    term_write(t, p);
    term_write(t, "$ ");
}

static void terminal_reset(terminal_t* t) {
    if (!t) return;
    t->in_len = 0; t->input[0] = '\0';
    t->line_count = 0; t->view_start = 0;
    term_newline(t);
    term_write(t, "KuvixOS Terminal v0.2\nhelp yaz.\n");
    term_prompt_out(t);
    term_scroll_to_bottom(t);
}

// ------------------------------------------------------------
// Commands Hooks
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
    t->in_len = 0; t->input[0] = '\0';
    t->line_count = 0; t->view_start = 0;
    term_newline(t);
    term_prompt_out(t);
    term_scroll_to_bottom(t);
    t->suppress_next_prompt = 1;
}

static void terminal_scroll_lines(terminal_t* t, int delta) {
    if (!t) return;
    int vis = term_max_visible_lines(t);
    if (t->line_count <= vis) { t->view_start = 0; return; }
    t->view_start += delta;
    if (t->view_start < 0) t->view_start = 0;
    int max_start = t->line_count - vis;
    if (t->view_start > max_start) t->view_start = max_start;
}

// ------------------------------------------------------------
// VTBL Callbacks
// ------------------------------------------------------------
static void terminal_on_create(app_t* app) {
    terminal_t* t = term(app);
    if (!t) return;
    memset(t, 0, sizeof(*t));
    strncpy(t->cwd, USER_DESKTOP_PATH, sizeof(t->cwd) - 1);
    t->lang = USER_LANG_EN;
    t->line_h = 14; t->cols = 80; t->rows = 20;
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

    if (t->view_start < 0) t->view_start = 0;
    if (t->view_start > t->line_count) t->view_start = t->line_count;

    int vis = term_max_visible_lines(t);
    int y = margin_y;
    for (int row = 0; row < vis; row++) {
        int li = t->view_start + row;
        if (li >= t->line_count) break;
        gfx_draw_text_utf8(margin_x, y, 0x00FF00, g_term_lines[li].text);
        y += t->line_h;
    }

    int content_rows = t->line_count - t->view_start;
    if (content_rows <= 0) content_rows = 1;
    if (content_rows > vis) content_rows = vis;

    int last_index = t->view_start + content_rows - 1;
    int base_y = margin_y + (content_rows - 1) * t->line_h;
    
    // ✅ DEĞİŞİKLİK: Eğer bir komut çalışıyorsa (progress bar gibi)
    // imlecin rastgele yerlerde gözükmesini engellemek için çizimi bitiriyoruz.
    if (t->is_running_cmd) return;

    int base_len = (last_index >= 0 && last_index < t->line_count) ? g_term_lines[last_index].len : 0;
    int x = margin_x + (base_len * 8);
    int y2 = base_y;

    int col = base_len;
    for (int i = 0; i < t->in_len; i++) {
        char s[2] = { t->input[i], 0 };
        gfx_draw_text_utf8(x, y2, 0x00FF00, s);
        x += 8; col++;
        if (col >= t->cols - 1) {
            col = 0; x = margin_x; y2 += t->line_h;
            if (y2 > (client.h - margin_y - t->line_h)) break;
        }
    }
    gfx_draw_text_utf8(x, y2, 0x00FF00, "_");
}

static void terminal_on_key(app_t* app, uint16_t key) {
    terminal_t* t = term(app);
    if (!t) return;
    uint8_t sc = (uint8_t)(key & 0xFF);
    if (sc & 0x80) return;
    if (sc == 0x49) { terminal_scroll_lines(t, -3); return; }
    if (sc == 0x51) { terminal_scroll_lines(t, +3); return; }

    if (sc == 0x1C) { // Enter
        commands_set_output(terminal_cmd_write, t);
        commands_set_clear(terminal_cmd_clear, t);
        commands_set_cwd(t->cwd);
        t->input[t->in_len] = '\0';
        term_write(t, t->input);
        term_write(t, "\n");

        // ✅ KOMUT BAŞLIYOR
        t->is_running_cmd = 1;
        commands_execute(t->input);
        t->is_running_cmd = 0;

        if (!t->suppress_next_prompt) term_prompt_out(t);
        t->suppress_next_prompt = 0;
        term_scroll_to_bottom(t);
        t->in_len = 0; t->input[0] = '\0';
        return;
    }

    char c = kbd_scancode_to_ascii(sc);
    if (c == '\b' || (uint8_t)c == 127) {
        if (t->in_len > 0) t->input[--t->in_len] = '\0';
        return;
    }
    if (c >= 32 && c <= 126) {
        if (t->in_len < TERM_INPUT_MAX - 1) {
            t->input[t->in_len++] = c;
            t->input[t->in_len] = '\0';
        }
    }
}

static void terminal_on_update(app_t* app) {
    terminal_t* t = term(app);
    if (!t) return;
    // Komut çalışırken veya veri akarken ekranın sürekli taze kalmasını sağlar
    if (t->is_running_cmd) app->wants_continuous_redraw = 1;
    else app->wants_continuous_redraw = 0;
}

static void terminal_on_destroy(app_t* app) { (void)app; }

const app_vtbl_t terminal_vtbl = {
    .on_create  = terminal_on_create,
    .on_draw    = terminal_on_draw,
    .on_mouse   = NULL, // mouse wheel gerekirse eklenebilir
    .on_key     = terminal_on_key,
    .on_update  = terminal_on_update,
    .on_destroy = terminal_on_destroy
};