#include <app/app.h>
#include <kernel/exec/kef_minimal.h>
#include <kernel/drivers/video/gfx.h>
#include <kernel/printk.h>
#include <lib/string.h>
#include <ui/wm.h>
#include <ui/color.h>

typedef struct {
    int window_id;
    int loaded;
    kef_minimal_app_t app;
} kef_minimal_state_t;

static kef_minimal_state_t g_kef;

static void kef_minimal_on_create(app_t* self) {
    memset(&g_kef, 0, sizeof(g_kef));
    g_kef.window_id = self->win_id;

    if (kef_minimal_load_file("/apps/hello.kef", &g_kef.app)) {
        g_kef.loaded = 1;
        printk("[KEF] loaded title='%s' text='%s'\n", g_kef.app.title, g_kef.app.text);
        wm_set_title(self->win_id, g_kef.app.title);
    } else {
        g_kef.loaded = 0;
        strcpy(g_kef.app.title, "KEF Minimal");
        strcpy(g_kef.app.text, "hello.kef load failed");
        wm_set_title(self->win_id, g_kef.app.title);
        printk("[KEF] load failed\n");
    }

    wm_invalidate_window(self->win_id);
}

static void kef_minimal_on_draw(app_t* self) {
    ui_rect_t c = wm_get_client_rect(self->win_id);

    // WM zaten origin'i client area'ya kuruyor
    gfx_fill_rect(0, 0, c.w, c.h, COLOR_WHITE);

    if (g_kef.loaded) {
        gfx_draw_text_utf8(12, 12, COLOR_BLACK, g_kef.app.text);
    } else {
        gfx_draw_text_utf8(12, 12, COLOR_BLACK, "KEF dosyasi yuklenemedi");
    }
}

const app_vtbl_t g_kef_minimal_vtbl = {
    .on_create = kef_minimal_on_create,
    .on_destroy = 0,
    .on_mouse = 0,
    .on_key = 0,
    .on_update = 0,
    .on_draw = kef_minimal_on_draw,
    .on_close_request = 0,
    .on_wheel = 0,
    .tabs_count = 0,
    .tabs_title = 0,
    .tabs_active = 0,
    .tabs_set_active = 0,
};