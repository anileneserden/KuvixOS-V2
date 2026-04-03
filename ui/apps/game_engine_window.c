#include <app/app.h>
#include <ui/wm.h>
#include <kernel/drivers/video/gfx.h>
#include <lib/string.h>
#include <stdint.h>

#include <ui/apps/game_engine_window.h>

static game_engine_window_t* game_engine_window(app_t* app) {
    return (app && app->user) ? (game_engine_window_t*)app->user : 0;
}

static void i32_to_str(int v, char* out) {
    char tmp[16];
    int n = 0;
    unsigned int x;

    if (v == 0) {
        out[0] = '0';
        out[1] = 0;
        return;
    }

    if (v < 0) {
        *out++ = '-';
        x = (unsigned int)(-v);
    } else {
        x = (unsigned int)v;
    }

    while (x > 0 && n < (int)sizeof(tmp)) {
        tmp[n++] = (char)('0' + (x % 10));
        x /= 10;
    }

    for (int i = 0; i < n; i++) out[i] = tmp[n - 1 - i];
    out[n] = 0;
}

static void game_engine_window_on_create(app_t* app) {
    game_engine_window_t* w = game_engine_window(app);
    ui_rect_t client;

    if (!w) return;

    w->win_id = app->win_id;
    client = wm_get_client_rect(w->win_id);
    w->cube_size = 64;
    w->cube_x = (client.w - w->cube_size) / 2;
    w->cube_y = (client.h - w->cube_size) / 2;
    w->vel_y = -1;
    w->update_counter = 0;
    w->initialized = 1;
    app->wants_continuous_redraw = 1;
}

static void game_engine_window_on_update(app_t* app) {
    game_engine_window_t* w = game_engine_window(app);
    if (!w || !w->initialized) return;

    w->update_counter++;
    w->cube_y += w->vel_y;
}

static void game_engine_window_on_draw(app_t* app) {
    game_engine_window_t* w = game_engine_window(app);
    ui_rect_t client;
    char counter_buf[16];
    char line_buf[48];

    if (!w) return;

    client = wm_get_client_rect(w->win_id);
    gfx_fill_rect(0, 0, client.w, client.h, 0x00171C22);
    gfx_fill_rect(w->cube_x, w->cube_y, w->cube_size, w->cube_size, 0x00D7903B);

    i32_to_str(w->update_counter, counter_buf);
    line_buf[0] = 0;
    strncpy(line_buf, "Update Counter: ", sizeof(line_buf) - 1);
    line_buf[sizeof(line_buf) - 1] = 0;
    strncat(line_buf, counter_buf, sizeof(line_buf) - 1 - (int)strlen(line_buf));

    gfx_draw_text_utf8(20, 20, 0x00EAEAEA, line_buf);
    gfx_draw_text_utf8(20, 44, 0x00999999, "Bu sayi artiyorsa on_update calisiyordur.");
}

static void game_engine_window_on_key(app_t* app, uint16_t keyev) {
    uint8_t sc = (uint8_t)(keyev & 0xFF);
    if ((sc & 0x80) == 0 && sc == 0x01) {
        wm_close_window(app->win_id);
    }
}

static void game_engine_window_on_destroy(app_t* app) {
    if (!app) return;
    app->wants_continuous_redraw = 0;
}

const app_vtbl_t game_engine_window_vtbl = {
    .on_create = game_engine_window_on_create,
    .on_destroy = game_engine_window_on_destroy,
    .on_mouse = 0,
    .on_key = game_engine_window_on_key,
    .on_update = game_engine_window_on_update,
    .on_draw = game_engine_window_on_draw,
    .on_close_request = 0,
    .on_wheel = 0,
    .tabs_count = 0,
    .tabs_title = 0,
    .tabs_active = 0,
    .tabs_set_active = 0
};