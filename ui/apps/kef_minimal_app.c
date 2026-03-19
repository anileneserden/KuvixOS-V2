#include <ui/apps/kef_minimal_app.h>

#include <kernel/exec/kef_json.h>
#include <kernel/drivers/video/gfx.h>
#include <kernel/printk.h>
#include <lib/string.h>
#include <ui/wm.h>
#include <ui/color.h>

static kef_minimal_state_t g_kef;

static void kef_minimal_on_create(app_t* self) {
    memset(&g_kef, 0, sizeof(g_kef));
    g_kef.window_id = self->win_id;

    if (kef_json_load_file("/apps/hello.json", &g_kef)) {
        g_kef.loaded = 1;
        wm_set_title(self->win_id, g_kef.title);
        printk("[KEFJSON] app loaded ok\n");
    } else {
        g_kef.loaded = 0;
        strcpy(g_kef.title, "KEF JSON");
        g_kef.width = 420;
        g_kef.height = 240;
        wm_set_title(self->win_id, g_kef.title);
        printk("[KEFJSON] app load failed\n");
    }

    wm_invalidate_window(self->win_id);
}

static void kef_minimal_on_draw(app_t* self) {
    (void)self;

    ui_rect_t c = wm_get_client_rect(g_kef.window_id);

    gfx_fill_rect(0, 0, c.w, c.h, COLOR_WHITE);

    if (!g_kef.loaded) {
        gfx_draw_text_utf8(12, 12, COLOR_BLACK, "hello.json yuklenemedi");
        return;
    }

    for (int i = 0; i < g_kef.widget_count; i++) {
        kef_widget_t* w = &g_kef.widgets[i];

        if (strcmp(w->type, "label") == 0) {
            gfx_draw_text_utf8(w->x, w->y, COLOR_BLACK, w->text);
        }
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