// kernel/ui/apps/kuvix_browser.c

#include <ui/apps/kuvix_browser.h>

#include <app/app.h>
#include <ui/wm.h>

#include <kernel/drivers/video/gfx.h>
#include <lib/string.h>
#include <stdint.h>
#include <stdbool.h>

// terminal'deki gibi
extern char kbd_scancode_to_ascii(uint8_t scancode);

// ------------------------------------------------------------
// helpers
// ------------------------------------------------------------
static kuvix_browser_t* br(app_t* app) {
    return (app && app->user) ? (kuvix_browser_t*)app->user : NULL;
}

static void br_set_status(kuvix_browser_t* b, const char* s) {
    if (!b) return;
    if (!s) s = "";
    strncpy(b->status, s, sizeof(b->status) - 1);
    b->status[sizeof(b->status) - 1] = 0;
}

static void br_set_url(kuvix_browser_t* b, const char* s) {
    if (!b) return;
    if (!s) s = "";
    strncpy(b->url, s, sizeof(b->url) - 1);
    b->url[sizeof(b->url) - 1] = 0;
    b->url_len = (int)strlen(b->url);
    if (b->url_len < 0) b->url_len = 0;
    if (b->url_len >= KBROWSER_URL_MAX) b->url_len = KBROWSER_URL_MAX - 1;
}

static void br_push_history(kuvix_browser_t* b, const char* s) {
    if (!b || !s || !s[0]) return;

    // back yaptıktan sonra yeni navigate -> forward kırp
    if (b->history_index < b->history_count - 1) {
        b->history_count = b->history_index + 1;
    }

    // doluysa sola kaydır
    if (b->history_count >= KBROWSER_HISTORY_MAX) {
        for (int i = 1; i < KBROWSER_HISTORY_MAX; i++) {
            strncpy(b->history[i - 1], b->history[i], KBROWSER_URL_MAX);
            b->history[i - 1][KBROWSER_URL_MAX - 1] = 0;
        }
        b->history_count = KBROWSER_HISTORY_MAX - 1;
        if (b->history_index > 0) b->history_index--;
    }

    strncpy(b->history[b->history_count], s, KBROWSER_URL_MAX - 1);
    b->history[b->history_count][KBROWSER_URL_MAX - 1] = 0;
    b->history_count++;
    b->history_index = b->history_count - 1;
}

static void br_navigate(kuvix_browser_t* b, const char* url) {
    if (!b) return;
    br_set_url(b, url);
    br_push_history(b, url);
    br_set_status(b, "Ready");
}

// ------------------------------------------------------------
// UI hitboxes (client coords)
// ------------------------------------------------------------
typedef struct { int x, y, w, h; } rect_t;

static bool pt_in_rect(int px, int py, rect_t r) {
    return (px >= r.x && py >= r.y && px < (r.x + r.w) && py < (r.y + r.h));
}

static rect_t r_btn_back(void)    { return (rect_t){ 8,  8, 28, 22 }; }
static rect_t r_btn_forward(void) { return (rect_t){ 40, 8, 28, 22 }; }
static rect_t r_btn_reload(void)  { return (rect_t){ 72, 8, 28, 22 }; }

static rect_t r_addr_bar(int cw) {
    int x = 110;
    int y = 8;
    int w = cw - x - 8;
    if (w < 80) w = 80;
    return (rect_t){ x, y, w, 22 };
}

static rect_t r_status_bar(int cw, int ch) {
    return (rect_t){ 0, ch - 20, cw, 20 };
}

static rect_t r_content(int cw, int ch) {
    // top toolbar: 38px, bottom status: 20px
    int y = 38;
    int h = ch - y - 20;
    if (h < 40) h = 40;
    return (rect_t){ 0, y, cw, h };
}

// ------------------------------------------------------------
// drawing primitives
// ------------------------------------------------------------
static void draw_button(rect_t r, const char* text, bool active) {
    uint32_t bg = active ? 0x303030 : 0x202020;
    uint32_t bd = active ? 0xFF6A00 : 0x505050;
    uint32_t fg = 0xFFFFFF;

    gfx_fill_rect(r.x, r.y, r.w, r.h, bg);
    gfx_draw_rect(r.x, r.y, r.w, r.h, bd);

    int tx = r.x + 6;
    int ty = r.y + 6;
    gfx_draw_text_utf8(tx, ty, fg, text);
}

static void draw_addrbar(rect_t r, const char* url, bool edit_mode) {
    uint32_t bg = 0x101010;
    uint32_t bd = edit_mode ? 0xFF6A00 : 0x505050;
    uint32_t fg = 0xFFFFFF;
    uint32_t hint = 0xA0A0A0;

    gfx_fill_rect(r.x, r.y, r.w, r.h, bg);
    gfx_draw_rect(r.x, r.y, r.w, r.h, bd);

    int tx = r.x + 6;
    int ty = r.y + 6;

    if (url && url[0]) gfx_draw_text_utf8(tx, ty, fg, url);
    else gfx_draw_text_utf8(tx, ty, hint, "type url...");

    if (edit_mode) {
        int len = (int)strlen(url ? url : "");
        int cx = tx + len * 8;
        if (cx > r.x + r.w - 10) cx = r.x + r.w - 10;
        gfx_draw_text_utf8(cx, ty, 0xFF6A00, "_");
    }
}

static void draw_toolbar_bg(int cw) {
    gfx_fill_rect(0, 0, cw, 38, 0x181818);
    gfx_draw_rect(0, 0, cw, 38, 0x303030);
}

static void draw_statusbar(rect_t r, const char* status) {
    gfx_fill_rect(r.x, r.y, r.w, r.h, 0x101010);
    gfx_draw_rect(r.x, r.y, r.w, r.h, 0x303030);
    gfx_draw_text_utf8(8, r.y + 6, 0xC0C0C0, status ? status : "");
}

// Tab başlıklarını titlebar'dan da gösteriyoruz ama content placeholder burada da yazsın
static const char* kb_tab_title(int idx) {
    // şimdilik sabit
    switch (idx) {
        case 0: return "Home";
        case 1: return "Docs";
        case 2: return "New";
        default: return "Tab";
    }
}

static void draw_content_placeholder(rect_t r, const char* url, int tab) {
    gfx_fill_rect(r.x, r.y, r.w, r.h, 0x1A1A1A);

    gfx_draw_text_utf8(r.x + 14, r.y + 14, 0xFFFFFF, "Kuvix Browser (template)");
    gfx_draw_text_utf8(r.x + 14, r.y + 32, 0xAAAAAA, "Engine: not implemented yet.");

    char line[220];
    line[0] = 0;
    strcat(line, "Tab: ");
    strcat(line, kb_tab_title(tab));
    strcat(line, "    URL: ");
    strcat(line, (url && url[0]) ? url : "(empty)");
    gfx_draw_text_utf8(r.x + 14, r.y + 54, 0xFF6A00, line);

    gfx_draw_text_utf8(r.x + 14, r.y + 78, 0x888888, "Tabs are on WINDOW TITLEBAR now (WM-driven).");
    gfx_draw_text_utf8(r.x + 14, r.y + 94, 0x666666, "Next: local: pages from VFS + simple text renderer + scroll.");
}

// ------------------------------------------------------------
// Tabs provider callbacks (WM titlebar uses these)
// ------------------------------------------------------------
static int kb_tabs_count(app_t* app) {
    (void)app;
    return 3; // şimdilik 3 tab
}

static const char* kb_tabs_title(app_t* app, int idx) {
    (void)app;
    return kb_tab_title(idx);
}

static int kb_tabs_active(app_t* app) {
    kuvix_browser_t* b = br(app);
    return b ? b->active_tab : 0;
}

static void kb_tabs_set_active(app_t* app, int idx) {
    kuvix_browser_t* b = br(app);
    if (!b) return;

    int n = kb_tabs_count(app);
    if (idx < 0) idx = 0;
    if (idx >= n) idx = n - 1;

    b->active_tab = idx;
    br_set_status(b, "Tab switched (titlebar)");
}

// ------------------------------------------------------------
// vtbl callbacks
// ------------------------------------------------------------
static void browser_on_create(app_t* app) {
    kuvix_browser_t* b = br(app);
    if (!b) return;

    memset(b, 0, sizeof(*b));

    b->active_tab = 0;
    b->addr_edit_mode = 0;

    br_set_url(b, "local:home");
    br_push_history(b, "local:home");
    br_set_status(b, "Ready");
}

static void browser_on_draw(app_t* app) {
    kuvix_browser_t* b = br(app);
    if (!b) return;

    ui_rect_t client = wm_get_client_rect(app->win_id);
    b->cw = client.w;
    b->ch = client.h;

    gfx_fill_rect(0, 0, client.w, client.h, 0x0B0B0B);

    // toolbar (tabs artık titlebar'da)
    draw_toolbar_bg(client.w);

    // toolbar buttons
    draw_button(r_btn_back(), "<", false);
    draw_button(r_btn_forward(), ">", false);
    draw_button(r_btn_reload(), "R", false);

    // address bar
    draw_addrbar(r_addr_bar(client.w), b->url, (b->addr_edit_mode != 0));

    // content
    rect_t rc = r_content(client.w, client.h);
    draw_content_placeholder(rc, b->url, b->active_tab);

    // status
    rect_t rs = r_status_bar(client.w, client.h);
    draw_statusbar(rs, b->status);
}

static void browser_on_mouse(app_t* app, int mx, int my, uint8_t pressed, uint8_t released, uint8_t buttons) {
    (void)released; (void)buttons;

    kuvix_browser_t* b = br(app);
    if (!b) return;

    if (!(pressed & 0x01)) return; // sadece left press

    // back
    if (pt_in_rect(mx, my, r_btn_back())) {
        if (b->history_count > 0 && b->history_index > 0) {
            b->history_index--;
            br_set_url(b, b->history[b->history_index]);
            br_set_status(b, "Back");
        }
        return;
    }

    // forward
    if (pt_in_rect(mx, my, r_btn_forward())) {
        if (b->history_count > 0 && b->history_index < b->history_count - 1) {
            b->history_index++;
            br_set_url(b, b->history[b->history_index]);
            br_set_status(b, "Forward");
        }
        return;
    }

    // reload
    if (pt_in_rect(mx, my, r_btn_reload())) {
        br_set_status(b, "Reload");
        return;
    }

    // address bar click -> edit mode
    if (pt_in_rect(mx, my, r_addr_bar(b->cw))) {
        b->addr_edit_mode = 1;
        br_set_status(b, "Editing address (Enter=go, Esc=cancel)");
        return;
    }

    // click elsewhere exits edit mode
    if (b->addr_edit_mode) {
        b->addr_edit_mode = 0;
        br_set_status(b, "Ready");
    }
}

static void browser_on_key(app_t* app, uint16_t key) {
    kuvix_browser_t* b = br(app);
    if (!b) return;

    uint8_t sc = (uint8_t)(key & 0xFF);
    if (sc & 0x80) return; // break ignore

    // edit mode değilse ignore
    if (!b->addr_edit_mode) return;

    // Enter
    if (sc == 0x1C) {
        b->addr_edit_mode = 0;
        br_navigate(b, b->url[0] ? b->url : "local:home");
        return;
    }

    // Esc
    if (sc == 0x01) {
        b->addr_edit_mode = 0;
        br_set_status(b, "Ready");
        return;
    }

    char c = kbd_scancode_to_ascii(sc);

    // Backspace
    if (c == '\b' || (uint8_t)c == 127) {
        if (b->url_len > 0) {
            b->url_len--;
            b->url[b->url_len] = '\0';
        }
        return;
    }

    // Printable
    if (c >= 32 && c <= 126) {
        if (b->url_len < KBROWSER_URL_MAX - 1) {
            b->url[b->url_len++] = c;
            b->url[b->url_len] = '\0';
        }
    }
}

static void browser_on_destroy(app_t* app) {
    (void)app;
}

const app_vtbl_t kuvix_browser_vtbl = {
    .on_create  = browser_on_create,
    .on_draw    = browser_on_draw,
    .on_mouse   = browser_on_mouse,
    .on_key     = browser_on_key,
    .on_destroy = browser_on_destroy,

    // ✅ titlebar tabs provider
    .tabs_count      = kb_tabs_count,
    .tabs_title      = kb_tabs_title,
    .tabs_active     = kb_tabs_active,
    .tabs_set_active = kb_tabs_set_active
};