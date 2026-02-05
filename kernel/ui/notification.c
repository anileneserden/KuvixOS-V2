#include <ui/notification.h>
#include <kernel/drivers/video/gfx.h>
#include <kernel/drivers/video/fb.h> // fb_get_width ve fb_get_height için
#include <lib/string.h>

static char notify_text[64];
static int notify_timer = 0;
static bool notify_visible = false;

// Bildirimi başlat
void notification_show(const char* text, uint32_t duration) {
    strncpy(notify_text, text, 63);
    notify_timer = duration;
    notify_visible = true;
}

void notification_draw(void) {
    if (!notify_visible || notify_timer <= 0) {
        notify_visible = false;
        return;
    }

    // Ekran boyutlarını dinamik alıyoruz
    int screen_w = fb_get_width();
    
    // Bildirim kutusu boyutları
    int w = 220;
    int h = 45;
    
    // Sağ üst köşe hesaplaması (Kenarlardan 10 piksel boşluk)
    int x = screen_w - w - 10; 
    int y = 10;

    // Arka Plan (Yarı saydam efektini simüle eden koyu renk)
    gfx_fill_rect(x, y, w, h, 0x1A1A1A); 
    
    // Kenarlık (Senin seçtiğin parlak mavi)
    gfx_draw_rect(x, y, w, h, 0x00AAFF); 

    // Bildirim İkonu ve Metin
    gfx_draw_text(x + 10, y + 15, 0x00AAFF, "[!] ");
    gfx_draw_text(x + 35, y + 15, 0xFFFFFF, notify_text);

    // Zamanlayıcıyı azalt
    notify_timer--;
}