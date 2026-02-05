#include <ui/topbar.h>
#include <kernel/drivers/video/gfx.h>
#include <kernel/drivers/video/fb.h>
#include <lib/string.h>

static int bar_h = 28; // Topbar yüksekliği

void topbar_init(void) {
    // Gerekirse statik değişkenler burada sıfırlanır
}

void topbar_draw(void) {
    int sw = fb_get_width();

    // 1. Bar Arka Planı (Hafif şeffaf görünümlü koyu gri/siyah)
    gfx_fill_rect(0, 0, sw, bar_h, 0x111111);
    
    // 2. Alt Çizgi (Ayrım çizgisi - İnce mavi veya gri)
    gfx_fill_rect(0, bar_h - 1, sw, 1, 0x00AAFF);

    // 3. "KuvixOS" Logosu/Yazısı
    gfx_draw_text(15, 7, 0xFFFFFF, "KuvixOS");

    // 4. Sağ Tarafa Bilgiler (Saat simülasyonu veya Aygıt Durumu)
    // Şimdilik sadece sabit bir metin koyalım
    gfx_draw_text(sw - 120, 7, 0xAAAAAA, "17:11  CPU: 2%");
}