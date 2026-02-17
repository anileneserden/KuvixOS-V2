#include <ui/topbar.h>
#include <ui/wm.h>
#include <kernel/drivers/video/gfx.h>
#include <kernel/drivers/video/fb.h>
#include <lib/string.h>

static int bar_h = 28;
static int btn_w = 120;   // pencere buton genişliği
static int btn_h = 22;

void topbar_init(void) {
}

static void draw_window_buttons(void) {
    int count = wm_get_count();
    int active = wm_get_active_id();
    int x = 120; // logo sonrası başlasın

    for (int zi = 0; zi < count; zi++) {
        int id = wm_get_z(zi);
        const ui_window_t* w = wm_get_window_ptr(id);
        if (!w) continue;

        uint32_t bg = 0x222222;
        uint32_t fg = 0xCCCCCC;

        if (id == active) {
            bg = 0x00AAFF;      // aktif pencere mavi
            fg = 0xFFFFFF;
        } else if (w->state == WIN_MINIMIZED) {
            bg = 0x1A1A1A;      // minimize pencere daha soluk
            fg = 0x777777;
        }

        gfx_fill_rect(x, 3, btn_w, btn_h, bg);

        // Başlık kısaltma
        char title_buf[32];
        strncpy(title_buf, w->title ? w->title : "Window", 30);
        title_buf[30] = 0;

        gfx_draw_text(x + 8, 8, fg, title_buf);

        x += btn_w + 6;
    }
}

void topbar_handle_mouse(int mx, int my) {
    // bar dışı
    if (my >= 28) return;

    int count = wm_get_count();
    int active = wm_get_active_id();

    int x = 120;
    int btn_w = 120;
    int btn_h = 22;

    for (int zi = 0; zi < count; zi++) {
        int id = wm_get_z(zi);
        const ui_window_t* w = wm_get_window_ptr(id);
        if (!w) continue;

        if (mx >= x && mx < x + btn_w && my >= 3 && my < 3 + btn_h) {
            if (w->state == WIN_MINIMIZED) {
                wm_restore(id);
            } else if (id == active) {
                wm_minimize(id);   // ✅ aktif olana tıklayınca minimize
            } else {
                wm_set_active(id);
            }
            return;
        }

        x += btn_w + 6;
    }
}

void topbar_draw(void) {
    int sw = fb_get_width();

    // Arkaplan
    gfx_fill_rect(0, 0, sw, bar_h, 0x111111);

    // Alt çizgi
    gfx_fill_rect(0, bar_h - 1, sw, 1, 0x00AAFF);

    // Logo
    gfx_draw_text(15, 7, 0xFFFFFF, "KuvixOS");

    // Açık pencereler
    draw_window_buttons();

    // Sağ saat
    gfx_draw_text(sw - 120, 7, 0xAAAAAA, "17:11  CPU: 2%");
}