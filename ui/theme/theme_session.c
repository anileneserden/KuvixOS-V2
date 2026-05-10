#include <ui/session.h>
#include <ui/theme/theme.h>
#include <ui/theme/theme_runtime.h>
#include <kernel/drivers/video/fb.h>
#include <kernel/drivers/video/gfx.h>
#include <kernel/printk.h>
#include <lib/string.h>

#include <ui/window/window.h>   // ui_window_t
#include <ui/window_chrome.h>   // layout helpers (zaten window draw içinde çağırıyorsan şart değil)

// basit mouse state (istersen gerçek mouse manager’dan al)
static int g_mx = 0, g_my = 0;

static ui_window_t g_preview;

void theme_session_init(void) {
    memset(&g_preview, 0, sizeof(g_preview));
    g_preview.x = 120;
    g_preview.y = 100;
    g_preview.w = 640;
    g_preview.h = 420;
    g_preview.title = "Theme Preview";
    g_preview.icon = 0;

    printk("[THEME_SESSION] init\n");
}

void theme_session_tick(void) {
    // Background (desktop yok)
    fb_draw_rect(0, 0, fb_get_width(), fb_get_height(), 0x101010);

    // örnek window çiz
    ui_window_draw(&g_preview, 1, g_mx, g_my);

    // küçük yardım yazısı
    gfx_draw_text_utf8(20, fb_get_height() - 20, 0xFFFFFF, "F5: Reload /persist/theme.kth");
}

void theme_session_handle_scancode(uint16_t sc) {
    // Senin scancode setine göre değişebilir:
    // örnek: F5 = 0x3F (set1) olabilir, sende farklıysa kendi mapping'inle düzelt.
    // En garanti: keyboard driverında F5'e bastığında debug log alıp sc kodunu bul.
    if (sc == 0x3F) {
        printk("[THEME_SESSION] reload requested\n");
        ui_theme_reload_from_disk();
    }
}