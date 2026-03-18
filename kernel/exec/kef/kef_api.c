#include <kernel/exec/kef_api.h>

#include <kernel/printk.h>
#include <kernel/drivers/video/gfx.h>
#include <ui/desktop.h>

#include <ui/apps/kef_host.h>   // ✅ kef_host_get_active + widgets
#include <lib/string.h>
#include <ui/wm.h>

#include <ui/theme.h>

extern uint32_t g_ticks_ms;

static int g_kef_active = 0;

// ----------------- core api -----------------
static void api_log_impl(const char* s) {
    if (!s) return;
    printk("%s", s);
}

static void api_fill_rect_impl(int x, int y, int w, int h, uint32_t color) {
    gfx_fill_rect(x, y, w, h, color);
}

static void api_text_impl(int x, int y, uint32_t color, const char* s) {
    if (!s) return;
    gfx_draw_text_utf8(x, y, color, s);
}

static void api_invalidate_impl(void) {
    static uint32_t last = 0;
    uint32_t now = g_ticks_ms;
    if (now == last) return;
    last = now;
    desktop_invalidate_full();
}

static void api_set_window_size_impl(int win_w, int win_h) {
    // Eski kodlar silindi. Artık window manager bunu otomatik yönetiyor.
}

// ----------------- widget create api -----------------
static void api_create_label_impl(int x, int y, const char* text) {
    // Eski kodlar silindi. Modern UI motoruna devredildi.
}

static void api_create_button_impl(int x, int y, int w, int h, const char* text, void (*onclick)(void*), void* user) {
    // Eski kodlar silindi. Modern UI motoruna devredildi.
}

// ----------------- export api -----------------
const kvx_api_t g_kvx_api = {
    .log             = api_log_impl,
    .fill_rect       = api_fill_rect_impl,
    .text            = api_text_impl,
    .invalidate      = api_invalidate_impl,
    .create_label    = api_create_label_impl,
    .create_button   = api_create_button_impl,
    .set_window_size = api_set_window_size_impl,
};

void kef_api_set_active(int on) { g_kef_active = on ? 1 : 0; }
int  kef_api_is_active(void)    { return g_kef_active; }