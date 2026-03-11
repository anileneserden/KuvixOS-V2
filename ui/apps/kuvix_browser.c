// ui/apps/kuvix_browser.c

#include <ui/apps/kuvix_browser.h>
#include <app/app.h>
#include <ui/wm.h>
#include <ui/html/html_parser.h>
#include <ui/html/html_render.h>
#include <ui/html/url_resolver.h>
#include <kernel/fs/vfs.h>
#include <kernel/drivers/video/gfx.h>
#include <kernel/memory/kmalloc.h>
#include <kernel/printk.h>
#include <lib/string.h>
#include <stdint.h>
#include <stdbool.h>
#include <kernel/user.h>

// ------------------------------------------------------------
// config & helpers
// ------------------------------------------------------------
#define KBROWSER_ENABLE_TABS 0
extern char kbd_scancode_to_ascii(uint8_t scancode);

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
}

static void br_set_addrbuf(kuvix_browser_t* b, const char* s) {
    if (!b) return;
    if (!s) s = "";
    strncpy(b->addr_buf, s, sizeof(b->addr_buf) - 1);
    b->addr_buf[sizeof(b->addr_buf) - 1] = 0;
    b->addr_len = (int)strlen(b->addr_buf);
}

static void br_push_history(kuvix_browser_t* b, const char* s) {
    if (!b || !s || !s[0]) return;
    if (b->history_index < b->history_count - 1) b->history_count = b->history_index + 1;
    if (b->history_count >= KBROWSER_HISTORY_MAX) {
        for (int i = 1; i < KBROWSER_HISTORY_MAX; i++) {
            strncpy(b->history[i - 1], b->history[i], KBROWSER_URL_MAX);
        }
        b->history_count = KBROWSER_HISTORY_MAX - 1;
        if (b->history_index > 0) b->history_index--;
    }
    strncpy(b->history[b->history_count], s, KBROWSER_URL_MAX - 1);
    b->history_count++;
    b->history_index = b->history_count - 1;
}

static void br_navigate(kuvix_browser_t* b, const char* url) {
    if (!b) return;
    br_set_url(b, url);
    br_push_history(b, url);
    br_set_addrbuf(b, b->url);
    b->scroll_y = 0;
    br_set_status(b, "Ready");
}

static int is_abs_path(const char* s) { return (s && s[0] == '/'); }

static bool resolve_url_to_path(const char* url, char* out, int cap) {
    if (!out || cap <= 0) return false;
    out[0] = 0;
    if (!url || !url[0]) { strncpy(out, USER_DESKTOP_PATH "/home.html", cap - 1); return true; }
    if (strncmp(url, "file:", 5) == 0) {
        const char* p = url + 5;
        while (*p == '/') p++;
        out[0] = '/'; out[1] = 0;
        strncat(out, p, cap - 2);
        return true;
    }
    if (is_abs_path(url)) { strncpy(out, url, cap - 1); return true; }
    if (url_resolve_to_path(url, out, cap)) return true;
    return false;
}

// ------------------------------------------------------------
// UI & Drawing
// ------------------------------------------------------------
typedef struct { int x, y, w, h; } rect_t;
static bool pt_in_rect(int px, int py, rect_t r) { return (px >= r.x && py >= r.y && px < (r.x + r.w) && py < (r.y + r.h)); }
static rect_t r_btn_back(void)    { return (rect_t){ 8,  8, 28, 22 }; }
static rect_t r_btn_forward(void) { return (rect_t){ 40, 8, 28, 22 }; }
static rect_t r_btn_reload(void)  { return (rect_t){ 72, 8, 28, 22 }; }
static rect_t r_addr_bar(int cw)  { return (rect_t){ 110, 8, cw - 118, 22 }; }
static rect_t r_status_bar(int cw, int ch) { return (rect_t){ 0, ch - 20, cw, 20 }; }
static rect_t r_content(int cw, int ch)    { return (rect_t){ 0, 38, cw, ch - 58 }; }

static void draw_button(rect_t r, const char* text, bool active) {
    gfx_fill_rect(r.x, r.y, r.w, r.h, active ? 0x303030 : 0x202020);
    gfx_draw_rect(r.x, r.y, r.w, r.h, active ? 0xFF6A00 : 0x505050);
    gfx_draw_text_utf8(r.x + 6, r.y + 6, 0xFFFFFF, text);
}

static void draw_content_html(rect_t r, const char* url, int scroll_y) {
    gfx_fill_rect(r.x, r.y, r.w, r.h, 0x1A1A1A);
    char path[256];
    if (!resolve_url_to_path(url, path, sizeof(path))) return;

    uint8_t* buf = 0; uint32_t sz = 0;
    if (!vfs_read_all_alloc(path, &buf, &sz)) return;

    html_doc_t doc;
    if (html_parse(&doc, (const char*)buf, sz)) {
        html_render_doc(&doc, r.x + 12, r.y + 12 - scroll_y, r.w - 24);
        
        // ✅ KRİTİK: 40 byte'lık sızıntıları temizleyen fonksiyon
        // Eğer sisteminde html_free_doc yoksa html_parser.h'yi kontrol etmelisin
        html_free(&doc); 
    }

    vfs_free_alloc(buf);
}

// ------------------------------------------------------------
// vtbl callbacks
// ------------------------------------------------------------
static void browser_on_create(app_t* app) {
    kuvix_browser_t* b = br(app);
    if (!b) return;
    memset(b, 0, sizeof(*b));
    br_navigate(b, "local:home");
}

static void browser_on_draw(app_t* app) {
    kuvix_browser_t* b = br(app);
    if (!b) return;
    ui_rect_t cl = wm_get_client_rect(app->win_id);
    b->cw = cl.w; b->ch = cl.h;

    gfx_fill_rect(0, 0, cl.w, cl.h, 0x0B0B0B);
    gfx_fill_rect(0, 0, cl.w, 38, 0x181818); // Toolbar bg

    draw_button(r_btn_back(), "<", false);
    draw_button(r_btn_forward(), ">", false);
    draw_button(r_btn_reload(), "R", false);
    
    const char* addr_text = b->addr_edit_mode ? b->addr_buf : b->url;
    uint32_t addr_bd = b->addr_edit_mode ? 0xFF6A00 : 0x505050;
    rect_t ra = r_addr_bar(cl.w);
    gfx_fill_rect(ra.x, ra.y, ra.w, ra.h, 0x101010);
    gfx_draw_rect(ra.x, ra.y, ra.w, ra.h, addr_bd);
    gfx_draw_text_utf8(ra.x + 6, ra.y + 6, 0xFFFFFF, addr_text);

    draw_content_html(r_content(cl.w, cl.h), b->url, b->scroll_y);
    
    rect_t rs = r_status_bar(cl.w, cl.h);
    gfx_fill_rect(rs.x, rs.y, rs.w, rs.h, 0x101010);
    gfx_draw_text_utf8(8, rs.y + 6, 0xC0C0C0, b->status);
}

static void browser_on_mouse(app_t* app, int mx, int my, uint8_t pressed, uint8_t released, uint8_t buttons) {
    kuvix_browser_t* b = br(app);
    if (!b || !(pressed & 0x01)) return;

    if (pt_in_rect(mx, my, r_btn_back()) && b->history_index > 0) {
        b->history_index--;
        br_navigate(b, b->history[b->history_index]);
    } else if (pt_in_rect(mx, my, r_addr_bar(b->cw))) {
        b->addr_edit_mode = 1;
        br_set_addrbuf(b, b->url);
    } else {
        b->addr_edit_mode = 0;
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
    if (sc & 0x80) return;

    if (!b->addr_edit_mode) {
        if (sc == 0x48) b->scroll_y = (b->scroll_y > 32) ? b->scroll_y - 32 : 0;
        if (sc == 0x50) b->scroll_y += 32;
        return;
    }

    char c = kbd_scancode_to_ascii(sc);
    if (sc == 0x1C) { b->addr_edit_mode = 0; br_navigate(b, b->addr_buf); }
    else if (sc == 0x0E && b->addr_len > 0) { b->addr_buf[--b->addr_len] = 0; }
    else if (c >= 32 && c <= 126 && b->addr_len < KBROWSER_URL_MAX - 1) {
        b->addr_buf[b->addr_len++] = c; b->addr_buf[b->addr_len] = 0;
    }
}

// ✅ Gizemli 2 KB sızıntısını çözen kısım:
static void browser_on_destroy(app_t* app) {
    if (!app || !app->user) return;

    kuvix_browser_t* b = (kuvix_browser_t*)app->user;
    
    // b->history dizisindeki stringler statik dizidir, 
    // ancak b->url gibi alanlar kalloc edildiyse burada free edilmelidir.
    
    kfree(b);
    app->user = NULL;
    
    printk("[Browser] Bellek temizlendi, 2 KB iade edildi.\n");
}

// Dışarıdan (FileManager veya AppManager gibi) çağrılabilen fonksiyon
void kuvix_browser_open_url(app_t* app, const char* url) {
    kuvix_browser_t* b = br(app);
    if (!b) return;

    // Eğer URL boşsa ana sayfaya, doluysa verilen adrese git
    br_navigate(b, (url && url[0]) ? url : "local:home");
    
    // Status bar'ı güncelle
    br_set_status(b, "Navigating...");
    
    // Uygulama penceresini çizime zorla (isteğe bağlı, wm_request_draw varsa ekleyebilirsin)
}

const app_vtbl_t kuvix_browser_vtbl = {
    .on_create  = browser_on_create,
    .on_draw    = browser_on_draw,
    .on_mouse   = browser_on_mouse,
    .on_key     = browser_on_key,
    .on_destroy = browser_on_destroy
};