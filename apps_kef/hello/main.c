#include <stdint.h>
#include <kernel/exec/kef_api.h>

static void btn_cb(void* u) {
    const kvx_api_t* api = (const kvx_api_t*)u;
    if (api && api->log) api->log("[HELLO] button clicked!\n");
}

static void on_create(const kvx_api_t* api) {
    if (api && api->create_label)
        api->create_label(10, 10, "Widget System OK");

    if (api && api->create_button)
        api->create_button(10, 40, 140, 28, "Click Me", btn_cb, (void*)api);
}

static void on_draw(const kvx_api_t* api) {
    if (!api) return;
   // if (api->fill_rect) api->fill_rect(0, 0, 220, 90, 0xE6E6E6);
    if (api->text)      api->text(10, 10, 0x000000, "HELLO.KEF");
}

static const kvx_kef_app_t g_app = {
    .on_create  = on_create,
    .on_draw    = on_draw,
    .on_key     = 0,
    .on_mouse   = 0,
    .on_destroy = 0
};

// ✅ YENİ ABI: entry "int" döndürür, vtbl'i out param'a yazar
__attribute__((used))
int _start(const kvx_api_t* api, kvx_kef_app_t* out_vtbl) {
    (void)api;
    if (!out_vtbl) return -1;
    *out_vtbl = g_app;   // struct kopyala
    return 0;
}