// kernel/ui/apps/kuvix_store.c
#include <ui/apps/kuvix_store.h>

#include <app/app.h>
#include <app/app_manager.h>
#include <ui/wm.h>

#include <kernel/drivers/video/gfx.h>
#include <kernel/fs/vfs.h>
#include <kernel/user.h>
#include <lib/string.h>
#include <kernel/printk.h>

#include <stdint.h>
#include <stdbool.h>

#include <ui/desktop.h>
#include <ui/desktop_icons.h>

extern void gfx_draw_text_utf8(int x, int y, uint32_t color, const char* s);

#define STORE_REPO_DIR "/system/repo/apps"

#define PAD    10
#define LIST_W 220
#define ROW_H  26
#define BTN_H  26
#define BTN_W  140

// ------------------------------------------------------------
// Global repo cache (heap yemesin diye)
// ------------------------------------------------------------
static store_item_t g_items[STORE_MAX_APPS];
static int g_count = 0;

// ------------------------------------------------------------
// Tiny helpers
// ------------------------------------------------------------
static bool ends_with(const char* s, const char* suf) {
    if (!s || !suf) return false;
    int sl = (int)strlen(s);
    int pl = (int)strlen(suf);
    if (sl < pl) return false;
    return strcmp(s + (sl - pl), suf) == 0;
}

static const char* base_name(const char* path) {
    const char* p = strrchr(path, '/');
    return p ? (p + 1) : path;
}

static void safe_copy(char* dst, int cap, const char* src) {
    if (!dst || cap <= 0) return;
    dst[0] = 0;
    if (!src) return;
    strncpy(dst, src, cap - 1);
    dst[cap - 1] = 0;
}

static int parse_int(const char* s) {
    int v = 0;
    while (*s >= '0' && *s <= '9') { v = v * 10 + (*s - '0'); s++; }
    return v;
}

// key="title=" -> read until newline
static bool parse_kv_line(const char* text, const char* key, char* out, int out_cap) {
    if (!text || !key || !out || out_cap <= 0) return false;
    const char* p0 = strstr(text, key);
    if (!p0) return false;
    const char* p = p0 + (int)strlen(key);

    int n = 0;
    while (*p && *p != '\n' && *p != '\r' && n + 1 < out_cap) {
        out[n++] = *p++;
    }
    out[n] = 0;
    return (n > 0);
}

// dir="/system/repo/apps" -> sadece "/system/repo/apps/<name>" kabul
static bool is_direct_child_of(const char* dir, const char* full) {
    if (!dir || !full) return false;

    int dlen = (int)strlen(dir);
    if (dlen <= 0) return false;

    // root special
    if (strcmp(dir, "/") == 0) {
        if (full[0] != '/') return false;
        const char* rest = full + 1;
        if (!rest[0]) return false;
        return (strchr(rest, '/') == 0);
    }

    if (strncmp(full, dir, (size_t)dlen) != 0) return false;
    if (full[dlen] != '/') return false;

    const char* rest = full + dlen + 1;
    if (!rest[0]) return false;
    return (strchr(rest, '/') == 0);
}

// Selected item detail read (desc/icon) from kapp_path
static void store_read_detail(const store_item_t* it,
                              char* out_desc, int desc_cap,
                              char* out_icon, int icon_cap)
{
    if (out_desc && desc_cap > 0) out_desc[0] = 0;
    if (out_icon && icon_cap > 0) out_icon[0] = 0;
    if (!it || !it->kapp_path[0]) return;

    uint8_t buf[512];
    uint32_t sz = 0;
    if (vfs_read_all(it->kapp_path, buf, sizeof(buf) - 1, &sz) != 1) return;
    buf[sz] = 0;

    if (out_desc && desc_cap > 0) parse_kv_line((const char*)buf, "desc=", out_desc, desc_cap);
    if (out_icon && icon_cap > 0) parse_kv_line((const char*)buf, "icon=", out_icon, icon_cap);
}

// ------------------------------------------------------------
// Repo load
// ------------------------------------------------------------
typedef struct { int dummy; } list_ctx_t;

static int store_list_cb(const char* full_path, uint32_t size, void* u) {
    (void)size; (void)u;

    if (g_count >= STORE_MAX_APPS) return 0;
    if (!full_path || !full_path[0]) return 1;

    if (!is_direct_child_of(STORE_REPO_DIR, full_path)) return 1;
    if (!ends_with(full_path, ".kapp")) return 1;

    uint8_t buf[512];
    uint32_t sz = 0;
    if (vfs_read_all(full_path, buf, sizeof(buf) - 1, &sz) != 1) return 1;
    buf[sz] = 0;

    store_item_t it;
    memset(&it, 0, sizeof(it));

    // kapp yolunu sakla
    safe_copy(it.kapp_path, STORE_PATH_MAX, full_path);

    // fallback title
    safe_copy(it.title, STORE_TITLE_MAX, base_name(full_path));

    // parse title/app_id
    parse_kv_line((const char*)buf, "title=", it.title, STORE_TITLE_MAX);

    char tmp[32];
    it.app_id = 0;
    if (parse_kv_line((const char*)buf, "app_id=", tmp, (int)sizeof(tmp))) {
        it.app_id = parse_int(tmp);
    }

    it.valid = (it.app_id > 0 && it.title[0] != 0);
    if (!it.valid) return 1;

    g_items[g_count++] = it;
    return 1;
}

static void store_reload(kuvix_store_t* st) {
    if (!st) return;

    g_count = 0;
    st->selected = -1;
    st->scroll = 0;

    // repo list
    list_ctx_t ctx;
    (void)ctx;
    vfs_list(STORE_REPO_DIR, store_list_cb, &ctx);

    st->count = g_count;
    if (g_count > 0) st->selected = 0;
}

// ------------------------------------------------------------
// Install shortcut (.ksf)
// ------------------------------------------------------------
static void build_desktop_ksf_path(char* out, int cap, const char* title) {
    if (!out || cap <= 0) return;
    out[0] = 0;

    safe_copy(out, cap, USER_DESKTOP_PATH);
    strncat(out, "/", cap - 1 - (int)strlen(out));

    // sanitize: space/'/' -> '_'
    char name[80];
    memset(name, 0, sizeof(name));
    safe_copy(name, (int)sizeof(name), title);

    for (int i = 0; name[i]; i++) {
        if (name[i] == ' ' || name[i] == '/') name[i] = '_';
    }

    strncat(out, name, cap - 1 - (int)strlen(out));
    strncat(out, ".ksf", cap - 1 - (int)strlen(out));
}

static void append_int_line(char* out, int out_cap, const char* key, int v) {
    if (!out || out_cap <= 0 || !key) return;

    strncat(out, key, out_cap - 1 - (int)strlen(out));

    // itoa
    char num[16];
    memset(num, 0, sizeof(num));
    int p = 0;

    if (v <= 0) {
        num[p++] = '0';
    } else {
        char tmp[16]; int tp = 0;
        while (v > 0 && tp < 15) { tmp[tp++] = (char)('0' + (v % 10)); v /= 10; }
        while (tp > 0 && p < 15) { num[p++] = tmp[--tp]; }
    }
    num[p] = 0;

    strncat(out, num, out_cap - 1 - (int)strlen(out));
    strncat(out, "\n", out_cap - 1 - (int)strlen(out));
}

static void install_shortcut(const store_item_t* it) {
    if (!it || !it->valid) return;

    // desktop dirs ensure (RAMFS)
    vfs_mkdir("/home");
    vfs_mkdir(USER_HOME_PATH);
    vfs_mkdir(USER_DESKTOP_PATH);

    char ksf_path[256];
    build_desktop_ksf_path(ksf_path, (int)sizeof(ksf_path), it->title);

    // read icon from kapp
    char icon[STORE_ICON_MAX];
    icon[0] = 0;
    store_read_detail(it, 0, 0, icon, (int)sizeof(icon));

    char ksf[256];
    memset(ksf, 0, sizeof(ksf));

    strncat(ksf, "title=", sizeof(ksf) - 1 - (int)strlen(ksf));
    strncat(ksf, it->title, sizeof(ksf) - 1 - (int)strlen(ksf));
    strncat(ksf, "\n", sizeof(ksf) - 1 - (int)strlen(ksf));

    append_int_line(ksf, (int)sizeof(ksf), "app_id=", it->app_id);

    if (icon[0]) {
        strncat(ksf, "icon=", sizeof(ksf) - 1 - (int)strlen(ksf));
        strncat(ksf, icon, sizeof(ksf) - 1 - (int)strlen(ksf));
        strncat(ksf, "\n", sizeof(ksf) - 1 - (int)strlen(ksf));
    }

    vfs_write_all(ksf_path, (const uint8_t*)ksf, (uint32_t)strlen(ksf));

    desktop_icons_init();
    desktop_icons_snap_all();
}

// ------------------------------------------------------------
// UI helpers
// ------------------------------------------------------------
static bool hit(int mx, int my, int x, int y, int w, int h) {
    return (mx >= x && mx < x + w && my >= y && my < y + h);
}

static void draw_button(int x, int y, const char* text, bool enabled) {
    uint32_t bg = enabled ? 0xFF0055AA : 0xFFB0B0B0;
    uint32_t fg = 0xFFFFFFFF;
    gfx_fill_rect(x, y, BTN_W, BTN_H, bg);
    gfx_draw_text_utf8(x + 10, y + 6, fg, text);
}

// ------------------------------------------------------------
// App callbacks
// ------------------------------------------------------------
static void store_on_create(app_t* app) {
    if (!app || !app->user) {
        printk("[Store] on_create: user=NULL (kmalloc fail?)\n");
        return;
    }

    kuvix_store_t* st = (kuvix_store_t*)app->user;
    memset(st, 0, sizeof(*st));

    store_reload(st);
    printk("[Store] on_create user=%p count=%d\n", app->user, st->count);
}

static void store_on_draw(app_t* app) {
    if (!app || !app->user) return;
    kuvix_store_t* st = (kuvix_store_t*)app->user;

    // safety
    if (st->count != g_count) st->count = g_count;
    if (st->count == 0) store_reload(st);

    if (st->selected >= st->count) st->selected = (st->count > 0) ? 0 : -1;
    if (st->scroll < 0) st->scroll = 0;
    if (st->scroll > st->count) st->scroll = st->count;

    ui_rect_t c = wm_get_client_rect(app->win_id);
    gfx_fill_rect(0, 0, c.w, c.h, 0xFFFFFFFF);

    // left list panel
    gfx_fill_rect(0, 0, LIST_W, c.h, 0xFFF3F3F3);
    gfx_fill_rect(LIST_W - 1, 0, 1, c.h, 0xFFCCCCCC);

    gfx_draw_text_utf8(10, 10, 0xFF333333, "KuvixStore");
    gfx_draw_text_utf8(10, 30, 0xFF777777, "Uygulamalar");

    int y0 = 60;
    int visible_rows = (c.h - y0 - 10) / ROW_H;
    if (visible_rows < 1) visible_rows = 1;

    int start = st->scroll;
    if (start < 0) start = 0;
    if (start > st->count) start = st->count;

    for (int i = 0; i < visible_rows; i++) {
        int idx = start + i;
        if (idx >= st->count) break;

        int y = y0 + i * ROW_H;
        bool sel = (idx == st->selected);

        if (sel) gfx_fill_rect(6, y, LIST_W - 12, ROW_H, 0xFF0055AA);

        uint32_t col = sel ? 0xFFFFFFFF : 0xFF222222;
        gfx_draw_text_utf8(14, y + 6, col, g_items[idx].title);
    }

    // right details
    int rx = LIST_W + PAD;
    gfx_draw_text_utf8(rx, 12, 0xFF333333, "Detay");

    if (st->selected >= 0 && st->selected < st->count) {
        store_item_t* it = &g_items[st->selected];

        // read desc/icon only for selected item
        char desc[STORE_DESC_MAX];
        char icon[STORE_ICON_MAX];
        store_read_detail(it, desc, (int)sizeof(desc), icon, (int)sizeof(icon));

        gfx_draw_text_utf8(rx, 40, 0xFF111111, it->title);
        gfx_draw_text_utf8(rx, 62, 0xFF666666, desc[0] ? desc : "(aciklama yok)");

        // buttons
        int by = 100;
        bool can_open = (it->app_id > 0);
        draw_button(rx, by, "Aç", can_open);
        draw_button(rx, by + BTN_H + 8, "Masaüstüne Ekle", true);

        if (icon[0]) {
            gfx_draw_text_utf8(rx, by + 2 * BTN_H + 24, 0xFF999999, icon);
        } else {
            gfx_draw_text_utf8(rx, by + 2 * BTN_H + 24, 0xFF999999, "(icon yok)");
        }

        // app_id info
        {
            char info[64];
            memset(info, 0, sizeof(info));
            strcpy(info, "app_id=");

            char num[16]; memset(num, 0, sizeof(num));
            int v = it->app_id, p = 0;
            if (v == 0) num[p++] = '0';
            while (v > 0 && p < 15) { num[p++] = (char)('0' + (v % 10)); v /= 10; }
            for (int a = 0, b = p - 1; a < b; a++, b--) { char t = num[a]; num[a] = num[b]; num[b] = t; }
            num[p] = 0;
            strcat(info, num);

            gfx_draw_text_utf8(rx, by + 2 * BTN_H + 44, 0xFF999999, info);
        }
    } else {
        gfx_draw_text_utf8(rx, 40, 0xFF666666, "Repo bos veya secim yok.");
        gfx_draw_text_utf8(rx, 62, 0xFF999999, "Manifest koy: /system/repo/apps/*.kapp");
    }
}

static void store_on_mouse(app_t* app, int mx, int my,
                           uint8_t pressed, uint8_t released, uint8_t buttons)
{
    (void)released; (void)buttons;
    if (!app || !app->user) return;
    if (!(pressed & 0x01)) return;

    kuvix_store_t* st = (kuvix_store_t*)app->user;

    // ✅ mx,my ekran coords geliyor → client-relative çevir (hit kaymasını bu çözer)
    printk("[Store] mouse raw=(%d,%d) client=(x=%d,y=%d)\n", mx, my);
    int lx = mx;
    int ly = my;

    // click list
    int y0 = 60;
    if (lx < LIST_W) {
        if (ly >= y0) {
            int idx = st->scroll + (ly - y0) / ROW_H;
            if (idx >= 0 && idx < st->count) st->selected = idx;
        }
        return;
    }

    // buttons on right
    if (st->selected < 0 || st->selected >= st->count) return;
    store_item_t* it = &g_items[st->selected];

    int rx = LIST_W + PAD;
    int by = 100;

    // Open
    if (hit(lx, ly, rx, by, BTN_W, BTN_H)) {
        if (it->app_id > 0) {
            appmgr_start_app(it->app_id);
            desktop_invalidate_full();
        }
        return;
    }

    // Install (desktop shortcut)
    if (hit(lx, ly, rx, by + BTN_H + 8, BTN_W, BTN_H)) {
        install_shortcut(it);
        return;
    }
}

static void store_on_key(app_t* app, uint16_t key) {
    if (!app || !app->user) return;
    kuvix_store_t* st = (kuvix_store_t*)app->user;

    uint8_t sc = (uint8_t)(key & 0xFF);

    // F5 reload
    if (sc == 0x3F) { store_reload(st); return; }

    // Up/Down (Set1: up=0x48 down=0x50)
    if (sc == 0x48) {
        if (st->selected > 0) st->selected--;
        if (st->selected < st->scroll) st->scroll = st->selected;
        return;
    }
    if (sc == 0x50) {
        if (st->selected + 1 < st->count) st->selected++;
        int visible_rows = 10;
        int bottom = st->scroll + (visible_rows - 1);
        if (st->selected > bottom) st->scroll = st->selected - (visible_rows - 1);
        if (st->scroll < 0) st->scroll = 0;
        return;
    }

    // Enter = Open
    if (sc == 0x1C) {
        if (st->selected >= 0 && st->selected < st->count) {
            store_item_t* it = &g_items[st->selected];
            if (it->app_id > 0) {
                appmgr_start_app(it->app_id);
                desktop_invalidate_full();
            }
        }
        return;
    }
}

const app_vtbl_t kuvix_store_vtbl = {
    .on_create  = store_on_create,
    .on_draw    = store_on_draw,
    .on_mouse   = store_on_mouse,
    .on_key     = store_on_key,
    .on_destroy = 0
};