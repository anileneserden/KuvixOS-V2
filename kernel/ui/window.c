#include <ui/window/window.h>
#include <ui/window_chrome.h>
#include <kernel/drivers/video/fb.h>
#include <kernel/drivers/video/gfx.h>

void ui_window_draw(const ui_window_t* win, int is_active, int mx, int my) {
    if (!win) return;
    ui_chrome_layout_t L = ui_chrome_layout(win);
    
    // Gövde ve Çerçeve
    uint32_t border_col = is_active ? 0x0078D7 : 0x808080;
    fb_draw_rect(win->x, win->y, win->w, win->h, 0xFFFFFF); // Arkaplan beyaz
    fb_draw_rect_outline(win->x, win->y, win->w, win->h, border_col);

    // Başlık Çubuğu
    fb_draw_rect(win->x, win->y, win->w, L.title_h, border_col);
    if (win->title) {
        gfx_draw_text(L.text_x, win->y + 6, 0xFFFFFF, win->title);
    }

    // Basit Buton Çizimleri
    fb_draw_rect(L.btn_close_x, L.btn_y, L.btn_size, L.btn_size, 0xE81123); // Kapat
    fb_draw_rect(L.btn_max_x, L.btn_y, L.btn_size, L.btn_size, 0xCCCCCC);   // Max
    fb_draw_rect(L.btn_min_x, L.btn_y, L.btn_size, L.btn_size, 0xCCCCCC);   // Min
}