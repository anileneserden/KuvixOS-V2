#include <ui/apps/kef_host.h>

#include <kernel/printk.h>
#include <kernel/memory/kmalloc.h>
#include <lib/string.h>

#include <kernel/exec/kef_api.h>
#include <app/app.h>

// runtime loader
int kef_load_app(const char* path, uint8_t** out_img, kvx_kef_app_t* out_vtbl);

static void host_on_create(app_t* a) {
    printk("[KEF-HOST] on_create a=%p user=%p win_id=%d\n", a, a ? a->user : 0, a ? a->win_id : -1);
    kef_host_t* h = (kef_host_t*)a->user;
    if (!h) return;

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

    if (h->vtbl.on_create) {
        kef_api_set_active(1);
        h->vtbl.on_create(&g_kvx_api);
        kef_api_set_active(0);
    }

    if (g_kvx_api.invalidate) g_kvx_api.invalidate();
}

static void host_on_draw(app_t* a) {
    kef_host_t* h = (kef_host_t*)a->user;
    if (!h) { printk("[KEF-HOST] draw: user NULL\n"); return; }
    // if (!h->vtbl_ready) { printk("[KEF-HOST] no vtbl (path=%s)\n", h->path); return; }

    // debug:
    // printk("[KEF-HOST] on_draw=%p\n", h->vtbl.on_draw);

    if (h->vtbl.on_draw) {
        kef_api_set_active(1);
        h->vtbl.on_draw(&g_kvx_api);
        kef_api_set_active(0);
    }
}

static void host_on_key(app_t* a, uint16_t keyev) {
    kef_host_t* h = (kef_host_t*)a->user;
    if (!h || !h->vtbl_ready) return;

    if (h->vtbl.on_key) {
        kef_api_set_active(1);
        h->vtbl.on_key(&g_kvx_api, keyev);
        kef_api_set_active(0);
    }
}

static void host_on_mouse(app_t* a, int mx, int my, uint8_t pr, uint8_t rel, uint8_t btn) {
    kef_host_t* h = (kef_host_t*)a->user;
    if (!h || !h->vtbl_ready) return;

    if (h->vtbl.on_mouse) {
        kef_api_set_active(1);
        h->vtbl.on_mouse(&g_kvx_api, mx, my, pr, rel, btn);
        kef_api_set_active(0);
    }
}

static void host_on_destroy(app_t* a) {
    kef_host_t* h = (kef_host_t*)a->user;
    if (!h) return;

    if (h->vtbl_ready && h->vtbl.on_destroy) {
        kef_api_set_active(1);
        h->vtbl.on_destroy(&g_kvx_api);
        kef_api_set_active(0);
    }

    h->vtbl_ready = 0;
    memset(&h->vtbl, 0, sizeof(h->vtbl));

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