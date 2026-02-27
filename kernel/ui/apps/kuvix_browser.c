// kernel/ui/apps/kuvix_browser.c

#include <ui/apps/kuvix_browser.h>

#include <app/app.h>
#include <ui/wm.h>

#include <ui/html/html_parser.h>
#include <ui/html/html_render.h>
#include <kernel/fs/vfs.h>

#include <kernel/drivers/video/gfx.h>
#include <lib/string.h>
#include <stdint.h>
#include <stdbool.h>

#include <kernel/user.h>

// ------------------------------------------------------------
// config
// ------------------------------------------------------------
#define KBROWSER_ENABLE_TABS 0

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
    b->scroll_y = 0; // ✅ yeni sayfada scroll reset
    br_set_status(b, "Ready");
}

// local:... -> VFS path (MVP)
static const char* local_to_path(const char* url) {
    // fallback her zaman masaüstündeki home.html
    if (!url || !url[0]) return USER_DESKTOP_PATH "/home.html";

    if (strncmp(url, "local:", 6) == 0) {
        const char* p = url + 6;

        if (strcmp(p, "home") == 0) return USER_DESKTOP_PATH "/home.html";
        if (strcmp(p, "docs") == 0) return USER_DESKTOP_PATH "/docs.html";
        if (strcmp(p, "new")  == 0) return USER_DESKTOP_PATH "/new.html";

        return USER_DESKTOP_PATH "/home.html";
    }

    // Bonus: adres çubuğuna direkt path yazarsan çalışsın
    // örnek: /home/anil/desktop/home.html (KuvixOS içindeki path!)
    if (url[0] == '/') return url;

    // şimdilik başka protokol yok
    return USER_DESKTOP_PATH "/home.html";
}

static bool browser_load_file(kuvix_browser_t* b, const char* path) {
    uint8_t* data = NULL;
    uint32_t size = 0;

    if (vfs_read_all(path, &data, &size) != 1) {
        br_set_status(b, "HTML load failed");
        return false;
    }

    // buffer içine kopyala
    if (size >= sizeof(b->html_buffer))
        size = sizeof(b->html_buffer) - 1;

    memcpy(b->html_buffer, data, size);
    b->html_buffer[size] = '\0';

    kfree(data);

    br_set_status(b, "Loaded");
    return true;
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

// HTML content draw (MVP local)
static void draw_content_html(rect_t r, const char* url, int scroll_y) {
    gfx_fill_rect(r.x, r.y, r.w, r.h, 0x1A1A1A);

    const char* path = local_to_path(url);

    uint8_t* buf = 0;
    uint32_t sz = 0;

    if (!vfs_read_all_alloc(path, &buf, &sz)) {
        gfx_draw_text_utf8(r.x + 14, r.y + 14, 0xFF4444, "HTML load failed:");
        gfx_draw_text_utf8(r.x + 14, r.y + 30, 0xAAAAAA, path);
        return;
    }

    html_doc_t doc;
    if (!html_parse(&doc, (const char*)buf, sz)) {
        gfx_draw_text_utf8(r.x + 14, r.y + 14, 0xFF4444, "HTML parse failed");
        vfs_free_alloc(buf);
        return;
    }

    // scroll -> y'yi kaydır
    int pad = 12;
    int draw_x = r.x + pad;
    int draw_y = r.y + pad - scroll_y;
    int draw_w = r.w - pad * 2;

    html_render_doc(&doc, draw_x, draw_y, draw_w);

    vfs_free_alloc(buf);
}

#if KBROWSER_ENABLE_TABS
// ------------------------------------------------------------
// Tabs provider callbacks (WM titlebar uses these)
// ------------------------------------------------------------

// Tab başlıklarını titlebar'dan da gösteriyoruz
static const char* kb_tab_title(int idx) {
    switch (idx) {
        case 0: return "Home";
        case 1: return "Docs";
        case 2: return "New";
        default: return "Tab";
    }
}

static int kb_tabs_count(app_t* app) {
    (void)app;
    return 3;
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

    if (idx == 0) br_navigate(b, "local:home");
    else if (idx == 1) br_navigate(b, "local:docs");
    else br_navigate(b, "local:new");

    br_set_status(b, "Tab switched (titlebar)");
}
#endif

// ------------------------------------------------------------
// vtbl callbacks
// ------------------------------------------------------------
static void browser_on_create(app_t* app) {
    kuvix_browser_t* b = br(app);
    if (!b) return;

    memset(b, 0, sizeof(*b));

    b->active_tab = 0;
    b->addr_edit_mode = 0;
    b->scroll_y = 0;

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

    // toolbar
    draw_toolbar_bg(client.w);

    // toolbar buttons
    draw_button(r_btn_back(), "<", false);
    draw_button(r_btn_forward(), ">", false);
    draw_button(r_btn_reload(), "R", false);

    // address bar
    draw_addrbar(r_addr_bar(client.w), b->url, (b->addr_edit_mode != 0));

    // content (HTML)
    rect_t rc = r_content(client.w, client.h);
    draw_content_html(rc, b->url, b->scroll_y);

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
            b->scroll_y = 0;
            br_set_status(b, "Back");
        }
        return;
    }

    // forward
    if (pt_in_rect(mx, my, r_btn_forward())) {
        if (b->history_count > 0 && b->history_index < b->history_count - 1) {
            b->history_index++;
            br_set_url(b, b->history[b->history_index]);
            b->scroll_y = 0;
            br_set_status(b, "Forward");
        }
        return;
    }

    // reload
    if (pt_in_rect(mx, my, r_btn_reload())) {
        b->scroll_y = 0;
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

    // ------------------------------------------------
    // Scroll keys (edit mode OFF iken çalışsın)
    // Up: 0x48, Down: 0x50, PgUp: 0x49, PgDn: 0x51
    // (E0 prefix'li gelirse setin farklıdır; terminal debug ile bakıp düzeltiriz)
    // ------------------------------------------------
    if (!b->addr_edit_mode) {
        int step = 16 * 2; // 2 satır

        if (sc == 0x48) { // Up
            b->scroll_y -= step;
            if (b->scroll_y < 0) b->scroll_y = 0;
            return;
        }
        if (sc == 0x50) { // Down
            b->scroll_y += step;
            return;
        }
        if (sc == 0x49) { // PgUp
            b->scroll_y -= step * 6;
            if (b->scroll_y < 0) b->scroll_y = 0;
            return;
        }
        if (sc == 0x51) { // PgDn
            b->scroll_y += step * 6;
            return;
        }

        // edit mode değilken diğer tuşlar: şimdilik yok
        return;
    }

    // ------------------------------
    // Address bar edit mode
    // ------------------------------

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

#if KBROWSER_ENABLE_TABS
    // titlebar tabs provider
    .tabs_count      = kb_tabs_count,
    .tabs_title      = kb_tabs_title,
    .tabs_active     = kb_tabs_active,
    .tabs_set_active = kb_tabs_set_active
#endif
};