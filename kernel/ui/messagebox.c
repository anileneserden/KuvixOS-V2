#include <ui/messagebox.h>
#include <kernel/drivers/video/gfx.h>
#include <lib/string.h>

messagebox_t sys_msgbox = { .visible = false, .w = 260, .h = 120 };

void msgbox_show(const char* title, const char* msg) {
    strncpy(sys_msgbox.title, title, 31);
    strncpy(sys_msgbox.message, msg, 63);
    
    // Ekranın ortasına yerleştir (fb_get_width/height varsayıyorum)
    sys_msgbox.x = (800 - sys_msgbox.w) / 2; 
    sys_msgbox.y = (600 - sys_msgbox.h) / 2;
    sys_msgbox.visible = true;
}

void msgbox_draw(void) {
    if (!sys_msgbox.visible) return;

    int x = sys_msgbox.x;
    int y = sys_msgbox.y;
    int w = sys_msgbox.w;
    int h = sys_msgbox.h;

    // Gölgeli arka plan
    gfx_fill_rect(x + 3, y + 3, w, h, 0x44000000); 
    gfx_fill_rect(x, y, w, h, 0xFFCCCCCC); // Gövde
    
    // Başlık Çubuğu
    gfx_fill_rect(x, y, w, 24, 0xFFCC0000); // Uyarı olduğu için kırmızımsı
    gfx_draw_text(x + 8, y + 6, 0xFFFFFFFF, sys_msgbox.title);
    
    // Mesaj
    gfx_draw_text(x + 15, y + 50, 0xFF000000, sys_msgbox.message);
    
    // Tamam Butonu
    int bx = x + (w/2) - 35;
    int by = y + h - 35;
    gfx_fill_rect(bx, by, 70, 25, 0xFF999999);
    gfx_draw_text(bx + 15, by + 6, 0xFF000000, "Tamam");
}

bool msgbox_handle_click(int mx, int my) {
    if (!sys_msgbox.visible) return false;

    int bx = sys_msgbox.x + (sys_msgbox.w/2) - 35;
    int by = sys_msgbox.y + sys_msgbox.h - 35;

    // "Tamam" butonuna basıldı mı?
    if (mx >= bx && mx <= bx + 70 && my >= by && my <= by + 25) {
        sys_msgbox.visible = false;
        return true;
    }
    return true; // MessageBox açıkken başka yere tıklanmasın (modal)
}