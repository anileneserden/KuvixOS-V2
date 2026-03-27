// kernel/ui/apps/settings.c
#include <ui/apps/settings.h>

#include <app/app.h>
#include <ui/wm.h>

#include <kernel/drivers/video/gfx.h>
#include <lib/string.h>
#include <stdint.h>
#include <stdbool.h>

#include <ui/ui_settings.h>
#include <kernel/memory/kmalloc.h>
#include <kernel/printk.h>

#include <ui/controls/combobox2.h>
#include <ui/controls/label2.h>

#include <ui/desktop.h>   // desktop_set_bg_color / desktop_invalidate_full

// ------------------------------------------------------------
// Layout
// ------------------------------------------------------------
#define PAD     10
#define LIST_W  220
#define ROW_H   26

static const char* k_pages[SETTINGS_PAGE_COUNT] = {
    "Genel",
    "Görünüm",
    "Depolama",
    "Hakkında"
};

// ------------------------------------------------------------
// Label2 helper
// ------------------------------------------------------------
static inline void draw_text2(int x, int y, uint32_t color, const char* text) {
    ui_label2_t l;
    ui_label2_init(&l, 0, (ui_point_t){ x, y }, color, text ? text : "");
    if (l.base.vtbl && l.base.vtbl->draw) l.base.vtbl->draw(&l.base);
}

// ------------------------------------------------------------
// small helpers
// ------------------------------------------------------------
static bool hit(int mx, int my, int x, int y, int w, int h) {
    return (mx >= x && mx < x + w && my >= y && my < y + h);
}

static void itoa_simple(int v, char* out, int cap) {
    if (!out || cap <= 0) return;
    out[0] = 0;

    char tmp[16];
    int n = 0;
    int neg = 0;

    if (v == 0) { out[0] = '0'; out[1] = 0; return; }
    if (v < 0) { neg = 1; v = -v; }

    while (v > 0 && n < (int)sizeof(tmp)) {
        tmp[n++] = (char)('0' + (v % 10));
        v /= 10;
    }

    int p = 0;
    if (neg && p < cap - 1) out[p++] = '-';
    while (n > 0 && p < cap - 1) out[p++] = tmp[--n];
    out[p] = 0;
}

static void draw_toggle(int x, int y, const char* label, bool on) {
    draw_text2(x, y + 6, 0xFF222222, label);

    int sw = 44;
    int sh = 18;
    int sx = x + 360; // fixed align, simple UI
    int sy = y + 4;

    gfx_fill_rect(sx, sy, sw, sh, on ? 0xFF00AA55 : 0xFFCCCCCC);
    gfx_draw_rect(sx, sy, sw, sh, 0xFF888888);

    int knob = sh - 4;
    int kx = on ? (sx + sw - knob - 2) : (sx + 2);
    int ky = sy + 2;
    gfx_fill_rect(kx, ky, knob, knob, 0xFFFFFFFF);
}

static int toggle_hit_x(void) { return (LIST_W + PAD + 360); }
static int toggle_hit_w(void) { return 44; }

// ------------------------------------------------------------
// Background combobox callback
// ------------------------------------------------------------
static void on_bg_change(void* user, int idx, const char* text)
{
    (void)user;
    (void)text;

    if (idx == 0) {
        desktop_set_bg_color(0xFF182838); // Kuvix Blue
    }
    else if (idx == 1) {
        desktop_set_bg_color(0xFF202020); // Dark Gray
    }
    else if (idx == 2) {
        desktop_set_bg_color(0xFF003344); // Ocean
    }
    else if (idx == 3) {
        desktop_set_bg_color(0xFF3A2A18); // Warm Brown
    }

    desktop_invalidate_full();
}

// ------------------------------------------------------------
// pages
// ------------------------------------------------------------
static void draw_page_general(int rx) {
    draw_text2(rx, 40, 0xFF111111, "Genel");
    draw_text2(rx, 62, 0xFF666666, "Temel sistem ayarları (şimdilik demo).");

    draw_text2(rx, 100, 0xFF444444, "- Dil: (yakında)");
    draw_text2(rx, 120, 0xFF444444, "- Saat: (yakında)");
    draw_text2(rx, 140, 0xFF444444, "- Bildirimler: (yakında)");
}

static void draw_page_appearance(settings_t* st, int rx) {
    draw_text2(rx, 40, 0xFF111111, "Görünüm");
    draw_text2(rx, 62, 0xFF666666, "Tema / masaüstü gorünümü");

    bool show_ext = ui_get_show_extensions();

    int y = 100;
    draw_toggle(rx, y, "Dosya uzantılarını göster", show_ext);
    draw_text2(rx, y + 36, 0xFF777777, "Not: Bu ayar desktop ikonlarini etkiler.");

    // Background combo
    draw_text2(rx, 140, 0xFF222222, "Masaüstü arkaplan rengi");

    int cbx = rx;
    int cby = 160;

    if (!st->cb_inited) {
        ui_combobox2_init(&st->cb_bg, cbx, cby, 260, 26);

        ui_combobox2_add_item(&st->cb_bg, "Kuvix Blue");
        ui_combobox2_add_item(&st->cb_bg, "Dark Gray");
        ui_combobox2_add_item(&st->cb_bg, "Ocean");
        ui_combobox2_add_item(&st->cb_bg, "Warm Brown");

        ui_combobox2_set_on_change(&st->cb_bg, on_bg_change, st);
        ui_combobox2_set_selected(&st->cb_bg, 0);

        st->cb_inited = 1;
    } else {
        st->cb_bg.x = cbx;
        st->cb_bg.y = cby;
        st->cb_bg.w = 260;
        st->cb_bg.h = 26;
    }

    ui_combobox2_draw(&st->cb_bg);
}

static void draw_page_storage(int rx) {
    draw_text2(rx, 40, 0xFF111111, "Depolama");
    draw_text2(rx, 62, 0xFF666666, "Heap (kmalloc) istatistikleri");

    kmalloc_stats_t s;
    kmalloc_get_stats(&s);

    char buf[64];
    int y = 100;

    draw_text2(rx, y, 0xFF444444, "Used (bytes):");
    itoa_simple((int)s.used_bytes, buf, (int)sizeof(buf));
    draw_text2(rx + 160, y, 0xFF111111, buf);
    y += 18;

    draw_text2(rx, y, 0xFF444444, "Free (bytes):");
    itoa_simple((int)s.free_bytes, buf, (int)sizeof(buf));
    draw_text2(rx + 160, y, 0xFF111111, buf);
    y += 18;

    draw_text2(rx, y, 0xFF444444, "Largest free:");
    itoa_simple((int)s.largest_free, buf, (int)sizeof(buf));
    draw_text2(rx + 160, y, 0xFF111111, buf);
    y += 18;

    draw_text2(rx, y, 0xFF444444, "Alloc count:");
    itoa_simple((int)s.alloc_count, buf, (int)sizeof(buf));
    draw_text2(rx + 160, y, 0xFF111111, buf);
    y += 18;

    draw_text2(rx, y, 0xFF444444, "Free count:");
    itoa_simple((int)s.free_count, buf, (int)sizeof(buf));
    draw_text2(rx + 160, y, 0xFF111111, buf);
    y += 22;

    draw_text2(rx, y, 0xFF777777,
        "İleride: RAM toplam, proses bazlı kullanım, fragmentation grafiği.");
}

static void draw_page_about(int rx) {
    draw_text2(rx, 40, 0xFF111111, "Hakkında");
    draw_text2(rx, 62, 0xFF666666, "KuvixOS V2 (dev build)");

    draw_text2(rx, 100, 0xFF444444, "UI: Window Manager + Desktop Icons");
    draw_text2(rx, 120, 0xFF444444, "FS: VFS (RAMFS/ToyFS/KVXFS deneyleri)");
    draw_text2(rx, 140, 0xFF444444, "Heap: kmalloc (first-fit + coalesce)");
}

// ------------------------------------------------------------
// app callbacks
// ------------------------------------------------------------
static void settings_on_create(app_t* app) {
    if (!app || !app->user) {
        printk("[Settings] on_create: user=NULL\n");
        return;
    }
    settings_t* st = (settings_t*)app->user;
    memset(st, 0, sizeof(*st));
    st->page = SETTINGS_PAGE_APPEARANCE;
    st->cb_inited = 0;
}

static void settings_on_draw(app_t* app) {
    if (!app || !app->user) return;
    settings_t* st = (settings_t*)app->user;

    ui_rect_t c = wm_get_client_rect(app->win_id);

    gfx_fill_rect(0, 0, c.w, c.h, 0xFFFFFFFF);

    gfx_fill_rect(0, 0, LIST_W, c.h, 0xFFF3F3F3);
    gfx_fill_rect(LIST_W - 1, 0, 1, c.h, 0xFFCCCCCC);

    draw_text2(10, 10, 0xFF333333, "Settings");
    draw_text2(10, 30, 0xFF777777, "Ayarlar");

    int y0 = 60;
    for (int i = 0; i < SETTINGS_PAGE_COUNT; i++) {
        int y = y0 + i * ROW_H;
        bool sel = (i == st->page);

        if (sel) gfx_fill_rect(6, y, LIST_W - 12, ROW_H, 0xFF0055AA);
        draw_text2(14, y + 6, sel ? 0xFFFFFFFF : 0xFF222222, k_pages[i]);
    }

    int rx = LIST_W + PAD;
    draw_text2(rx, 12, 0xFF333333, "Detay");

    switch ((settings_page_t)st->page) {
        case SETTINGS_PAGE_GENERAL:     draw_page_general(rx);         break;
        case SETTINGS_PAGE_APPEARANCE:  draw_page_appearance(st, rx);  break;
        case SETTINGS_PAGE_STORAGE:     draw_page_storage(rx);         break;
        case SETTINGS_PAGE_ABOUT:       draw_page_about(rx);           break;
        default:                        draw_page_general(rx);         break;
    }

    draw_text2(rx, c.h - 18, 0xFF999999, "İpuçları: Sol listeden seç. ESC: çıkış (yakında)");
}

static void settings_on_mouse(app_t* app, int mx, int my,
                              uint8_t pressed, uint8_t released, uint8_t buttons)
{
    if (!app || !app->user) return;
    settings_t* st = (settings_t*)app->user;

    // ✅ combobox mouse handling (önce)
    if (st->page == SETTINGS_PAGE_APPEARANCE && st->cb_inited) {
        if (ui_combobox2_handle_mouse(&st->cb_bg, mx, my, pressed, released, buttons)) {
            desktop_invalidate_full();
            return;
        }
    }

    // left click only
    if (!(pressed & 0x01)) return;

    int lx = mx;
    int ly = my;

    // categories
    int y0 = 60;
    if (lx < LIST_W) {
        if (ly >= y0) {
            int idx = (ly - y0) / ROW_H;
            if (idx >= 0 && idx < SETTINGS_PAGE_COUNT) {
                st->page = idx;
                desktop_invalidate_full();
            }
        }
        return;
    }

    // appearance toggle
    if (st->page == SETTINGS_PAGE_APPEARANCE) {
        int ty = 100;
        int sx = toggle_hit_x();
        int sy = ty + 4;
        int sw = toggle_hit_w();
        int sh = 18;

        if (hit(lx, ly, sx, sy, sw, sh)) {
            ui_toggle_show_extensions();
            desktop_invalidate_full();
            return;
        }
    }
}

static void settings_on_key(app_t* app, uint16_t key) {
    if (!app || !app->user) return;
    settings_t* st = (settings_t*)app->user;

    uint8_t sc = (uint8_t)(key & 0xFF);
    if (sc & 0x80) return;

    if (sc == 0x48) { // up
        if (st->page > 0) st->page--;
        desktop_invalidate_full();
        return;
    }
    if (sc == 0x50) { // down
        if (st->page + 1 < SETTINGS_PAGE_COUNT) st->page++;
        desktop_invalidate_full();
        return;
    }

    if (sc == 0x1C) { // enter
        if (st->page == SETTINGS_PAGE_APPEARANCE) {
            ui_toggle_show_extensions();
            desktop_invalidate_full();
        }
        return;
    }
}

static void settings_on_destroy(app_t* app) { (void)app; }

const app_vtbl_t settings_vtbl = {
    .on_create  = settings_on_create,
    .on_draw    = settings_on_draw,
    .on_mouse   = settings_on_mouse,
    .on_key     = settings_on_key,
    .on_destroy = settings_on_destroy
};