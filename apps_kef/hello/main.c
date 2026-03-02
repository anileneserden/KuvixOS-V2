#include <stdint.h>
#include <kernel/exec/kef_api.h>
#include "main.designer.h"
#include "kefstring.h"

typedef struct {
    const kvx_api_t* api;
    int clicks;
} hello_state_t;

static hello_state_t g_state;

// designer.c bunu çağıracak
void hello_button_onclick(void* user) {
    hello_state_t* st = (hello_state_t*)user;
    if (!st) return;
    st->clicks++;
    if (st->api && st->api->invalidate) st->api->invalidate();
}

static void on_create(const kvx_api_t* api) {
    if (api && api->log) api->log("[HELLO] on_create\n");
    g_state.api = api;
    g_state.clicks = 0;
    api->log("[HELLO] calling set_window_size now\n");
    if (api->set_window_size) api->set_window_size(400, 250);
    else api->log("[HELLO] set_window_size is NULL\n");

    ui_build(api, &g_state);

    if (api && api->invalidate) api->invalidate();
}

static void itoa10(int v, char* out, int out_cap) {
    if (!out || out_cap <= 0) return;
    out[0] = 0;

    if (v <= 0) {
        if (out_cap > 1) { out[0] = '0'; out[1] = 0; }
        return;
    }

    char tmp[16];
    int tp = 0;
    while (v > 0 && tp < 15) { tmp[tp++] = (char)('0' + (v % 10)); v /= 10; }

    int p = 0;
    while (tp > 0 && p < out_cap - 1) out[p++] = tmp[--tp];
    out[p] = 0; 
}

static void on_draw(const kvx_api_t* api) {
    if (!api) return;

    if (api->fill_rect) api->fill_rect(0, 0, 500, 300, 0xE6E6E6);

    char num[16];
    itoa10(g_state.clicks, num, sizeof(num));

    char msg[64];
    memset(msg, 0, sizeof(msg));
    strcat(msg, "Clicks: ");
    strcat(msg, num);

    // api->log("[HELLO] on_draw\n");

    if (api->text) api->text(10, 90, 0x000000, msg);
}

static const kvx_kef_app_t g_app = {
    .on_create  = on_create,
    .on_draw    = on_draw,
    .on_key     = 0,
    .on_mouse   = 0,
    .on_destroy = 0
};

// ✅ Entry mutlaka görünür olmalı
__attribute__((used))
int _start(const kvx_api_t* api, kvx_kef_app_t* out_vtbl) {
    (void)api;
    if (!out_vtbl) return -1;
    *out_vtbl = g_app;
    return 0;
}