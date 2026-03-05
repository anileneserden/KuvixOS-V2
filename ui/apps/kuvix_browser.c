#include <ui/apps/kuvix_browser.h>

#include <app/app.h>
#include <ui/wm.h>

#include <ui/html/html_parser.h>
#include <ui/html/html_render.h>
#include <ui/html/url_resolver.h>
#include <ui/html/css.h>

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
    b->scroll_y = 0; // yeni sayfada scroll reset
    br_set_status(b, "Ready");
}

static int is_abs_path(const char* s) {
    return (s && s[0] == '/');
}

// local fallback (resolver bulamazsa)
static const char* local_to_path(const char* url) {
    if (!url || !url[0]) return USER_DESKTOP_PATH "/home.html";

    if (is_abs_path(url)) return url;

    if (strncmp(url, "local:", 6) == 0) {
        const char* p = url + 6;
        if (strcmp(p, "home") == 0) return USER_DESKTOP_PATH "/home.html";
        if (strcmp(p, "docs") == 0) return USER_DESKTOP_PATH "/docs.html";
        if (strcmp(p, "new")  == 0) return USER_DESKTOP_PATH "/new.html";
        return USER_DESKTOP_PATH "/home.html";
    }

    return 0;
}

// URL -> PATH resolve (hosts + local fallback)
static bool resolve_url_to_path(const char* url, char* out, int cap) {
    if (!out || cap <= 0) return false;
    out[0] = 0;

    if (!url || !url[0]) {
        strncpy(out, USER_DESKTOP_PATH "/home.html", cap - 1);
        out[cap - 1] = 0;
        return true;
    }

    // 0) file: scheme  (file:/path veya file:///path)
    if (strncmp(url, "file:", 5) == 0) {
        const char* p = url + 5;

        // file:///home/... gibi durumlarda fazla slash’ları kırp
        while (*p == '/') p++;

        // başına '/' koyarak absolute path üret
        if (*p) {
            if (cap >= 2) {
                out[0] = '/';
                out[1] = 0;
                strncat(out, p, (size_t)cap - strlen(out) - 1);
            }
        } else {
            strncpy(out, "/", cap - 1);
            out[cap - 1] = 0;
        }
        return true;
    }

    // 1) absolute path
    if (is_abs_path(url)) {
        strncpy(out, url, cap - 1);
        out[cap - 1] = 0;
        return true;
    }

    // 2) resolver (home.local -> /.../home.html, deneme.local -> /.../index.html)
    // url_resolve_to_path senin vhosts.conf / hosts sistemini kullanacak
    if (url_resolve_to_path(url, out, cap)) {
        return true;
    }

    // 3) fallback local:
    const char* p = local_to_path(url);
    if (p) {
        strncpy(out, p, cap - 1);
        out[cap - 1] = 0;
        return true;
    }

    return false;
}

// ------------------------------------------------------------
// UI hitboxes (CLIENT coords)
// ------------------------------------------------------------
typedef struct { int x, y, w, h; } rect_t;

// ------------------------------------------------------------
// Directory listing (MVP)
// ------------------------------------------------------------
typedef struct {
    char dir[VFS_PATH_MAX];
    char entries[128][VFS_PATH_MAX];
    int  count;
} br_dirlist_t;

static const char* basename_of(const char* path) {
    if (!path) return "";
    const char* p = strrchr(path, '/');
    return p ? (p + 1) : path;
}

static int br_list_cb(const char* path, uint32_t size, void* u) {
    (void)size;
    br_dirlist_t* dl = (br_dirlist_t*)u;
    if (!dl) return 0;
    if (dl->count >= 128) return 0;

    strncpy(dl->entries[dl->count], path, VFS_PATH_MAX - 1);
    dl->entries[dl->count][VFS_PATH_MAX - 1] = 0;
    dl->count++;
    return 1;
}

static void build_index_path(const char* dir, char* out, int cap) {
    if (!out || cap <= 0) return;
    out[0] = 0;
    if (!dir || !dir[0]) return;

    strncpy(out, dir, cap - 1);
    out[cap - 1] = 0;

    int n = (int)strlen(out);
    if (n > 0 && out[n - 1] != '/') {
        if (n + 1 < cap) {
            out[n] = '/';
            out[n + 1] = 0;
        }
    }
    strncat(out, "index.html", (size_t)cap - strlen(out) - 1);
}

static void draw_dir_listing(rect_t r, const char* dir_path, int scroll_y) {
    gfx_fill_rect(r.x, r.y, r.w, r.h, 0x1A1A1A);

    br_dirlist_t dl;
    memset(&dl, 0, sizeof(dl));
    strncpy(dl.dir, dir_path ? dir_path : "/", sizeof(dl.dir) - 1);
    dl.dir[sizeof(dl.dir) - 1] = 0;

    vfs_list(dl.dir, br_list_cb, &dl);

    int pad = 12;
    int x = r.x + pad;
    int y = r.y + pad - scroll_y;

    gfx_draw_text_utf8(x, y, 0xFFFFFF, "Directory:");
    gfx_draw_text_utf8(x + 96, y, 0xAAAAAA, dl.dir);
    y += 18;

    if (dl.count == 0) {
        gfx_draw_text_utf8(x, y, 0xAAAAAA, "(empty)");
        return;
    }

    for (int i = 0; i < dl.count; i++) {
        if (y > r.y + r.h - 12) break;
        if (y < r.y - 16) { y += 16; continue; }

        const char* full = dl.entries[i];
        const char* name = basename_of(full);

        vfs_stat_t st;
        int is_dir = (vfs_stat(full, &st) && st.type == VFS_T_DIR);

        if (is_dir) gfx_draw_text_utf8(x, y, 0xFF6A00, "[DIR]");
        else        gfx_draw_text_utf8(x, y, 0x808080, "     ");

        gfx_draw_text_utf8(x + 44, y, 0xFFFFFF, name);
        y += 16;
    }
}

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
    gfx_draw_text_utf8(r.x + 6, r.y + 6, fg, text);
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

// HTML content draw (local/hosts)
static void draw_content_html(kuvix_browser_t* b, rect_t r, const char* url, int scroll_y) {
    gfx_fill_rect(r.x, r.y, r.w, r.h, 0x1A1A1A);

    char path[VFS_PATH_MAX];
    if (!resolve_url_to_path(url, path, (int)sizeof(path))) {
        gfx_draw_text_utf8(r.x + 14, r.y + 14, 0xFF4444, "Unsupported URL");
        gfx_draw_text_utf8(r.x + 14, r.y + 30, 0xAAAAAA, url ? url : "(null)");
        return;
    }

    // ✅ file: prefix
    if (strncmp(path, "file:", 5) == 0) {
        memmove(path, path + 5, strlen(path + 5) + 1);
        if (!path[0]) strncpy(path, "/", sizeof(path) - 1);
    }

    // ✅ file/dir ayrımı
    vfs_stat_t st;
    if (!vfs_stat(path, &st)) {
        gfx_draw_text_utf8(r.x + 14, r.y + 14, 0xFF4444, "Not found:");
        gfx_draw_text_utf8(r.x + 14, r.y + 30, 0xAAAAAA, path);
        return;
    }

    // ✅ KLASÖR ise: index.html dene, yoksa listing çiz
    if (st.type == VFS_T_DIR) {
        char idx[VFS_PATH_MAX];
        build_index_path(path, idx, (int)sizeof(idx));

        vfs_stat_t st2;
        if (vfs_stat(idx, &st2) && st2.type == VFS_T_FILE) {
            strncpy(path, idx, sizeof(path) - 1);
            path[sizeof(path) - 1] = 0;
        } else {
            draw_dir_listing(r, path, scroll_y);
            return;
        }
    }

    // ✅ FILE (HTML)
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

    if (doc.title && doc.title_len) {
        char tbuf[64];
        int n = (doc.title_len < 63) ? (int)doc.title_len : 63;
        memcpy(tbuf, doc.title, n);
        tbuf[n] = 0;
        br_set_status(b, tbuf);
    }

    int pad = 12;
    int draw_x = r.x + pad;
    int draw_y = r.y + pad - scroll_y;
    int draw_w = r.w - pad * 2;

    // ---- CSS (MVP) ----
    css_stylesheet_t sheet;
    css_parse(
        "div{color:white;background-color:#222222;}"
        ".box{background-color:#ff8800;color:black;}"
        "a{color:#33A0FF;}",
        &sheet
    );
    css_apply_styles(doc.root, &sheet);
    html_render_doc(&doc, draw_x, draw_y, draw_w);

    vfs_free_alloc(buf);
}

void kuvix_browser_open_url(app_t* app, const char* url) {
    kuvix_browser_t* b = br(app);
    if (!b) return;
    br_navigate(b, (url && url[0]) ? url : "local:home");
    br_set_status(b, "Opened from Fileman");
}

#if KBROWSER_ENABLE_TABS
static const char* kb_tab_title(int idx) {
    switch (idx) {
        case 0: return "Home";
        case 1: return "Docs";
        case 2: return "New";
        default: return "Tab";
    }
}
static int kb_tabs_count(app_t* app) { (void)app; return 3; }
static const char* kb_tabs_title(app_t* app, int idx) { (void)app; return kb_tab_title(idx); }
static int kb_tabs_active(app_t* app) { kuvix_browser_t* b = br(app); return b ? b->active_tab : 0; }
static void kb_tabs_set_active(app_t* app, int idx) {
    kuvix_browser_t* b = br(app);
    if (!b) return;
    if (idx < 0) idx = 0;
    if (idx >= 3) idx = 2;

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

    // edit buffer init
    strncpy(b->addr_buf, b->url, sizeof(b->addr_buf) - 1);
    b->addr_buf[sizeof(b->addr_buf) - 1] = 0;
    b->addr_len = (int)strlen(b->addr_buf);
}

static void browser_on_draw(app_t* app) {
    kuvix_browser_t* b = br(app);
    if (!b) return;

    ui_rect_t client = wm_get_client_rect(app->win_id);
    b->cw = client.w;
    b->ch = client.h;

    gfx_fill_rect(0, 0, client.w, client.h, 0x0B0B0B);

    draw_toolbar_bg(client.w);
    draw_button(r_btn_back(), "<", false);
    draw_button(r_btn_forward(), ">", false);
    draw_button(r_btn_reload(), "R", false);

    const char* addr_text = b->addr_edit_mode ? b->addr_buf : b->url;
    draw_addrbar(r_addr_bar(client.w), addr_text, (b->addr_edit_mode != 0));

    rect_t rc = r_content(client.w, client.h);
    // content HER ZAMAN committed url ile çizilir
    draw_content_html(b, rc, b->url, b->scroll_y);

    rect_t rs = r_status_bar(client.w, client.h);
    draw_statusbar(rs, b->status);
}

static void browser_on_mouse(app_t* app, int mx, int my,
                            uint8_t pressed, uint8_t released, uint8_t buttons)
{
    (void)released; (void)buttons;

    kuvix_browser_t* b = br(app);
    if (!b) return;

    if (!(pressed & 0x01)) return;

    // ✅ GLOBAL -> CLIENT
    ui_rect_t client = wm_get_client_rect(app->win_id);
    (void)client;
    int lx = mx;
    int ly = my;

    if (pt_in_rect(lx, ly, r_btn_back())) {
        if (b->history_count > 0 && b->history_index > 0) {
            b->history_index--;
            br_set_url(b, b->history[b->history_index]);
            b->scroll_y = 0;
            br_set_status(b, "Back");
        }
        return;
    }

    if (pt_in_rect(lx, ly, r_btn_forward())) {
        if (b->history_count > 0 && b->history_index < b->history_count - 1) {
            b->history_index++;
            br_set_url(b, b->history[b->history_index]);
            b->scroll_y = 0;
            br_set_status(b, "Forward");
        }
        return;
    }

    if (pt_in_rect(lx, ly, r_btn_reload())) {
        b->scroll_y = 0;
        br_set_status(b, "Reload");
        return;
    }

    if (pt_in_rect(lx, ly, r_addr_bar(b->cw))) {
        b->addr_edit_mode = 1;

        // edit buffer = current url
        strncpy(b->addr_buf, b->url, sizeof(b->addr_buf) - 1);
        b->addr_buf[sizeof(b->addr_buf) - 1] = 0;
        b->addr_len = (int)strlen(b->addr_buf);

        br_set_status(b, "Editing address (Enter=go, Esc=cancel)");
        return;
    }

    if (b->addr_edit_mode) {
        b->addr_edit_mode = 0;
        br_set_status(b, "Ready");
    }
}

static bool key_is_probably_ascii(uint8_t k) {
    // printable + backspace + enter
    if (k >= 32 && k <= 126) return true;
    if (k == '\b' || k == 127) return true;
    if (k == '\n' || k == '\r') return true;
    return false;
}

static void browser_on_key(app_t* app, uint16_t key) {
    kuvix_browser_t* b = br(app);
    if (!b) return;

    uint8_t sc = (uint8_t)(key & 0xFF);

    // break (key release) ignore
    if (sc & 0x80) return;

    // edit mode OFF => scroll keys (scancode)
    if (!b->addr_edit_mode) {
        int step = 32;
        if (sc == 0x48) { b->scroll_y -= step; if (b->scroll_y < 0) b->scroll_y = 0; return; } // up
        if (sc == 0x50) { b->scroll_y += step; return; }                                       // down
        if (sc == 0x49) { b->scroll_y -= step * 6; if (b->scroll_y < 0) b->scroll_y = 0; return; } // pgup
        if (sc == 0x51) { b->scroll_y += step * 6; return; }                                       // pgdn
        return;
    }

    // ---- edit mode ON ----

    // Enter
    if (sc == 0x1C) {
        b->addr_edit_mode = 0;
        br_navigate(b, b->addr_buf[0] ? b->addr_buf : "local:home");
        return;
    }

    // Esc
    if (sc == 0x01) {
        b->addr_edit_mode = 0;
        br_set_status(b, "Ready");
        return;
    }

    // Backspace (set1: 0x0E)
    if (sc == 0x0E) {
        if (b->addr_len > 0) {
            b->addr_len--;
            b->addr_buf[b->addr_len] = '\0';
        }
        return;
    }

    // Printable char via your layout mapper
    char c = kbd_scancode_to_ascii(sc);
    if (c >= 32 && c <= 126) {
        if (b->addr_len < KBROWSER_URL_MAX - 1) {
            b->addr_buf[b->addr_len++] = c;
            b->addr_buf[b->addr_len] = '\0';
        }
        return;
    }
}

static void browser_on_destroy(app_t* app) { (void)app; }

const app_vtbl_t kuvix_browser_vtbl = {
    .on_create  = browser_on_create,
    .on_draw    = browser_on_draw,
    .on_mouse   = browser_on_mouse,
    .on_key     = browser_on_key,
    .on_destroy = browser_on_destroy,
#if KBROWSER_ENABLE_TABS
    .tabs_count      = kb_tabs_count,
    .tabs_title      = kb_tabs_title,
    .tabs_active     = kb_tabs_active,
    .tabs_set_active = kb_tabs_set_active
#endif
};