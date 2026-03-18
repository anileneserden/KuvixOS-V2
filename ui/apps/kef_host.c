#include <ui/apps/kef_host.h>
#include <ui/window/window.h>
#include <ui/wm.h>

#include <kernel/printk.h>
#include <kernel/memory/kmalloc.h>
#include <lib/string.h>

#include <kernel/exec/kef_api.h>
#include <app/app.h>

// runtime loader
int kef_load_app(const char* path, uint8_t** out_img, kvx_kef_app_t* out_vtbl);

// ------------------------------------------------------------
// Active host (API create_* bunun üzerinden widget ekleyecek)
// ------------------------------------------------------------
static kef_host_t* g_active_host = 0;

kef_host_t* kef_host_get_active(void) {
    return g_active_host;
}

// ------------------------------------------------------------
// Helpers: widget çizimi + click dispatch
// ------------------------------------------------------------
static void kef_widgets_draw(kef_host_t* h) {
    if (!h) return;

    for (int i = 0; i < h->widget_count; i++) {
        widget_t* w = &h->widgets[i];
        if (!w->visible) continue;

        if (w->type == WIDGET_LABEL) {
            if (g_kvx_api.text)
                g_kvx_api.text(w->x, w->y, 0x000000, w->text);
        } else if (w->type == WIDGET_BUTTON) {
            uint32_t bg = 0xD0D0D0;
            uint32_t border = 0x808080;
            uint32_t textc = 0x000000;

            if (w->hovered) bg = 0xC8C8C8;
            if (w->pressed) bg = 0xB8B8B8;

            // background
            g_kvx_api.fill_rect(w->x, w->y, w->w, w->h, bg);

            // border (1px)
            g_kvx_api.fill_rect(w->x, w->y, w->w, 1, border);
            g_kvx_api.fill_rect(w->x, w->y + w->h - 1, w->w, 1, border);
            g_kvx_api.fill_rect(w->x, w->y, 1, w->h, border);
            g_kvx_api.fill_rect(w->x + w->w - 1, w->y, 1, w->h, border);

            // text padding
            g_kvx_api.text(w->x + 8, w->y + 8, textc, w->text);
        }
    }
}

static void kef_widgets_mouse_down(kef_host_t* h, int mx, int my) {
    if (!h) return;

    for (int i = 0; i < h->widget_count; i++) {
        widget_t* w = &h->widgets[i];
        if (!w->visible) continue;

        if (w->type != WIDGET_BUTTON) continue;

        if (mx >= w->x && mx <= w->x + w->w &&
            my >= w->y && my <= w->y + w->h) {

            if (w->on_click) {
                // callback app içinde; user paramı app'in verdiği şey
                w->on_click(w->user);
            }

            // tek buton tetikleyelim (istersen çokluya izin verirsin)
            return;
        }
    }
}

static int kef_widgets_update_hover(kef_host_t* h, int mx, int my) {
    int changed = 0;

    for (int i = 0; i < h->widget_count; i++) {
        widget_t* w = &h->widgets[i];
        if (!w->visible) continue;
        if (w->type != WIDGET_BUTTON) continue;

        int inside = (mx >= w->x && mx <= w->x + w->w &&
                      my >= w->y && my <= w->y + w->h);

        int nh = inside ? 1 : 0;
        if (w->hovered != nh) {
            w->hovered = nh;
            changed = 1;
        }
    }

    return changed;
}

static int kef_widgets_mouse_event(kef_host_t* h, int mx, int my, uint8_t pr, uint8_t rel) {
    if (!h) return 0;

    int changed = kef_widgets_update_hover(h, mx, my);

    for (int i = 0; i < h->widget_count; i++) {
        widget_t* w = &h->widgets[i];
        if (!w->visible) continue;
        if (w->type != WIDGET_BUTTON) continue;

        int inside = w->hovered;

        if (pr & 0x01) {
            int np = inside ? 1 : 0;
            if (w->pressed != np) { w->pressed = np; changed = 1; }
        }

        if (rel & 0x01) {
            int fire = (w->pressed && inside);
            if (w->pressed) { w->pressed = 0; changed = 1; }
            if (fire && w->on_click) w->on_click(w->user);
        }
    }

    return changed;
}

// ------------------------------------------------------------
// App callbacks
// ------------------------------------------------------------
static void host_on_create(app_t* a) {
    printk("[KEF-HOST] on_create a=%p user=%p win_id=%d\n", a, a ? a->user : 0, a ? a->win_id : -1);
    kef_host_t* h = (kef_host_t*)a->user;
    if (!h) return;

    // yeni pencere açıldı -> widget temizle
    h->widget_count = 0;
    memset(h->widgets, 0, sizeof(h->widgets));

    uint8_t* img = 0;
    kvx_kef_app_t vtbl;
    memset(&vtbl, 0, sizeof(vtbl));

    int rc = kef_load_app(h->path, &img, &vtbl);
    if (rc != 0) {
        printk("[KEF-HOST] load failed rc=%d path=%s\n", rc, h->path);
        return;
    }

    printk("[KEF-HOST] load rc=%d\n", rc);

    h->img = img;
    h->vtbl = vtbl;
    h->vtbl_ready = 1;

    printk("[KEF-HOST] vtbl: on_create=%p on_draw=%p on_mouse=%p on_key=%p\n",
       h->vtbl.on_create, h->vtbl.on_draw, h->vtbl.on_mouse, h->vtbl.on_key);

    printk("[KEF-HOST] api: create_label=%p create_button=%p text=%p fill_rect=%p\n",
        g_kvx_api.create_label, g_kvx_api.create_button, g_kvx_api.text, g_kvx_api.fill_rect);

    // aktif host set (create_label / create_button bunu kullanır)
    g_active_host = h;

    if (h->vtbl.on_create) {
        kef_api_set_active(1);
        h->vtbl.on_create(&g_kvx_api);
        kef_api_set_active(0);
    }

    printk("[KEF-HOST] after on_create: widget_count=%d\n", h->widget_count);

    g_active_host = 0;

    if (g_kvx_api.invalidate) g_kvx_api.invalidate();
}

static void host_on_draw(app_t* a) {
    kef_host_t* h = (kef_host_t*)a->user;
    if (!h || !h->vtbl_ready) return;

    // ✅ client rect'i al
    ui_rect_t cr = wm_get_client_rect(a->win_id);

    g_active_host = h;

    // ✅ önce client arka planını temizle (window_bg gibi)
    // Not: burada renk sabit, istersen theme'den al
    if (g_kvx_api.fill_rect) {
        g_kvx_api.fill_rect(0, 0, cr.w, cr.h, 0xE8E8E8);
    }

    // sonra app draw
    if (h->vtbl.on_draw) {
        kef_api_set_active(1);
        h->vtbl.on_draw(&g_kvx_api);
        kef_api_set_active(0);
    }

    // widget overlay
    kef_widgets_draw(h);

    g_active_host = 0;
}

static void host_on_key(app_t* a, uint16_t keyev) {
    kef_host_t* h = (kef_host_t*)a->user;
    if (!h || !h->vtbl_ready) return;

    g_active_host = h;

    if (h->vtbl.on_key) {
        kef_api_set_active(1);
        h->vtbl.on_key(&g_kvx_api, keyev);
        kef_api_set_active(0);
    }

    g_active_host = 0;
}

static void host_on_mouse(app_t* a, int mx, int my, uint8_t pr, uint8_t rel, uint8_t btn) {
    (void)btn;

    kef_host_t* h = (kef_host_t*)a->user;
    if (!h || !h->vtbl_ready) return;

    g_active_host = h;

    // önce app'e ver
    if (h->vtbl.on_mouse) {
        kef_api_set_active(1);
        h->vtbl.on_mouse(&g_kvx_api, mx, my, pr, rel, btn);
        kef_api_set_active(0);
    }

    int changed = kef_widgets_mouse_event(h, mx, my, pr, rel);

    // hover değişimi veya click/release olunca repaint
    if (changed || (pr & 0x01) || (rel & 0x01)) {
        if (g_kvx_api.invalidate) g_kvx_api.invalidate();
    }

    g_active_host = 0;
}

static void host_on_destroy(app_t* a) {
    kef_host_t* h = (kef_host_t*)a->user;
    if (!h) return;

    g_active_host = h;

    if (h->vtbl_ready && h->vtbl.on_destroy) {
        kef_api_set_active(1);
        h->vtbl.on_destroy(&g_kvx_api);
        kef_api_set_active(0);
    }

    g_active_host = 0;

    h->vtbl_ready = 0;
    memset(&h->vtbl, 0, sizeof(h->vtbl));

    h->widget_count = 0;
    memset(h->widgets, 0, sizeof(h->widgets));

    if (h->img) {
        kfree(h->img);
        h->img = 0;
    }
}

const app_vtbl_t kef_host_vtbl = {
    .on_create  = host_on_create,
    .on_destroy = host_on_destroy,
    .on_mouse   = host_on_mouse,
    .on_key     = host_on_key,
    .on_update  = 0,
    .on_draw    = host_on_draw,
    .on_close_request = 0,
    .on_wheel   = 0
};