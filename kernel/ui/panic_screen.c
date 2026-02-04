#include <kernel/drivers/video/gfx.h>
#include <kernel/drivers/video/fb.h>

void draw_panic_screen(const char* message) {
    int sw = fb_get_width();
    int sh = fb_get_height();

    // 1. Arka plana (Back Buffer) çizim yap
    gfx_fill_rect(0, 0, sw, sh, 0x0000AA);
    gfx_draw_text(40, 40, 0xFFFFFF, ":(");
    gfx_draw_text(40, 80, 0xFFFFFF, "KuvixOS bir sorunla karsilasti.");
    
    gfx_fill_rect(40, 140, sw - 80, 100, 0x000088);
    gfx_draw_text(60, 160, 0xFFFF00, "DURDURMA KODU:");
    gfx_draw_text(200, 160, 0xFFFFFF, message);

    // 2. KRİTİK ADIM: Çizilenleri ekrana bas!
    // Eğer fb_present() fonksiyonun yoksa, ismi fb_swap(), fb_flush() 
    // veya g_fb_present() olabilir. desktop.c'nin en altına bak.
    fb_present(); 
}