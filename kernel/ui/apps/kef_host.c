#include <ui/apps/kef_host.h>

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
            if (g_kvx_api.fill_rect)
                g_kvx_api.fill_rect(w->x, w->y, w->w, w->h, 0xCCCCCC);

            if (g_kvx_api.text)
                g_kvx_api.text(w->x + 4, w->y + 6, 0x000000, w->text);
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
    if (!h) { printk("[KEF-HOST] draw: user NULL\n"); return; }
    if (!h->vtbl_ready) { printk("[KEF-HOST] no vtbl (path=%s)\n", h->path); return; }

    static int once = 0;
    if (!once) {
        once = 1;
        printk("[KEF-HOST] draw: on_draw=%p widget_count=%d\n", h->vtbl.on_draw, h->widget_count);
    }

    g_active_host = h;

    // 1) app draw (isterse background çizsin)
    if (h->vtbl.on_draw) {
        kef_api_set_active(1);
        h->vtbl.on_draw(&g_kvx_api);
        kef_api_set_active(0);
    }

    // 2) widget overlay draw (label/button burada çizilir)
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
    (void)rel; (void)btn;

    kef_host_t* h = (kef_host_t*)a->user;
    if (!h || !h->vtbl_ready) return;

    g_active_host = h;

    // önce app'e ver
    if (h->vtbl.on_mouse) {
        kef_api_set_active(1);
        h->vtbl.on_mouse(&g_kvx_api, mx, my, pr, rel, btn);
        kef_api_set_active(0);
    }

    // sonra widget hit-test (sol tık pressed)
    if (pr & 0x01) {
        kef_widgets_mouse_down(h, mx, my);
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