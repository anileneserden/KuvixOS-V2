#include <ui/window/window.h>
#include <ui/window_chrome.h>
#include <kernel/drivers/video/fb.h>  // <--- Bunu ekle/güncelle
#include <ui/theme.h>
#include <font/font8x8_basic.h>

// Mevcut hatalı satırları şunlarla değiştir:
#include "bitmaps/icons/icon_close_16.h"
#include "bitmaps/icons/icon_max_16.h"
#include "bitmaps/icons/icon_min_16.h"

// Başlangıçta ikonları belleğe hazırla
void ui_windows_init(void) {
    icon_close_16_init();
    icon_max_16_init();
    icon_min_16_init();
}

static void draw_text8(int x, int y, uint32_t color, const char* s) {
    while (*s) {
        const uint8_t* glyph = font8x8_basic[(unsigned char)*s];
        for (int row = 0; row < 8; row++) {
            for (int col = 0; col < 8; col++) {
                if (glyph[row] & (1u << (7 - col))) fb_putpixel(x + col, y + row, color);
            }
        }
        x += 8; s++;
    }
}

static uint32_t darken(uint32_t argb, int amount) {
    int r = ((argb >> 16) & 0xFF) - amount;
    int g = ((argb >> 8) & 0xFF) - amount;
    int b = (argb & 0xFF) - amount;
    return (argb & 0xFF000000) | ((r<0?0:r)<<16) | ((g<0?0:g)<<8) | (b<0?0:b);
}

void ui_window_draw(const ui_window_t* win, int is_active, int mx, int my) {
    if (!win) return;
    const ui_theme_t* th = ui_get_theme();
    ui_chrome_layout_t L = ui_chrome_layout(win);
    wm_hittest_t hover = ui_chrome_hittest(win, mx, my);

    // Arka plan ve Çerçeve
    uint32_t border = is_active ? 0x5078DC : th->window_border;
    fb_draw_rect(win->x, win->y, win->w, win->h, th->window_bg);
    fb_draw_rect_outline(win->x, win->y, win->w, win->h, border);

    // Başlık Çubuğu
    fb_draw_rect(win->x, win->y, win->w, L.title_h, th->window_title_bg);
    if (win->title) draw_text8(win->x + 8, win->y + 8, th->window_title_text, win->title);

    // Butonlar ve İkonlar
    if (hover == HT_BTN_CLOSE) fb_draw_rect(L.btn_close_x, L.btn_y, L.btn_size, L.btn_size, darken(th->window_title_bg, 40));
    
    fb_blit_argb_key(L.btn_min_x, L.btn_y, 16, 16, g_icon_min_16, 0);
    fb_blit_argb_key(L.btn_max_x, L.btn_y, 16, 16, g_icon_max_16, 0);
    fb_blit_argb_key(L.btn_close_x, L.btn_y, 16, 16, g_icon_close_16, 0);
}