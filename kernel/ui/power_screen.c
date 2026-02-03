#include <ui/power_screen.h>
#include <kernel/drivers/video/fb.h>
#include <kernel/drivers/video/gfx.h>  // Merkezi çizim fonksiyonları için
#include <font/font8x16_tr.h>          // Yeni font tanımları için
#include <kernel/power.h>
#include <stdint.h>

/* --------- Yardımcı Fonksiyonlar --------- */

static int kstrlen(const char* s) { 
    int n = 0; 
    while(s && s[n]) n++; 
    return n; 
}

// Meşgul bekleme (Busy-wait) animasyon hızı için
static void sleep_ms_busy(uint32_t ms) {
    while (ms--) {
        for (volatile uint32_t i = 0; i < 60000; i++) {
            __asm__ __volatile__("pause");
        }
    }
}

static void power_anim(const char* msg, uint32_t seconds, int do_reboot) {
    const char spin[4] = {'|','/','-','\\'};

    int sw = (int)fb_get_width();
    int sh = (int)fb_get_height();

    // Harf genişliği 8px, yükseklik 16px.
    int msg_w = kstrlen(msg) * 8;
    int x_msg = (sw - msg_w) / 2;
    int y_msg = sh / 2 - 40; // Yazıyı biraz daha yukarı çektik

    for (int t = (int)seconds; t > 0; t--) {
        for (int f = 0; f < 10; f++) {
            fb_clear(fb_rgb(0, 0, 0));

            // Merkezi gfx_draw_text kullanımı (8x16 destekli)
            gfx_draw_text(x_msg, y_msg, fb_rgb(255, 255, 255), msg);

            // Geri sayım metni hazırlığı
            char cnt[8];
            cnt[0] = (char)('0' + t); 
            cnt[1] = '.'; cnt[2] = '.'; cnt[3] = '.'; cnt[4] = 0;

            int x_cnt = (sw - 4 * 8) / 2;
            int y_cnt = y_msg + 24; // 16px harf + 8px boşluk
            gfx_draw_text(x_cnt, y_cnt, fb_rgb(200, 200, 200), cnt);

            // Spinner (Dönen çizgi) animasyonu
            char sp[2] = { spin[(t * 10 + f) & 3], 0 };
            int x_sp = (sw / 2) - 4;
            int y_sp = y_cnt + 24; // Geri sayımın altına yerleştir
            gfx_draw_text(x_sp, y_sp, fb_rgb(200, 200, 200), sp);

            fb_present();
            sleep_ms_busy(10);
        }
    }

    // Son kare: Sadece ana mesajı göster
    fb_clear(fb_rgb(0, 0, 0));
    gfx_draw_text(x_msg, y_msg, fb_rgb(255, 255, 255), msg);
    fb_present();
    sleep_ms_busy(200);

    // Donanım seviyesinde kapatma/reboot
    if (do_reboot) power_reboot();
    else           power_shutdown();
}

/* --------- Dışarıdan Çağrılan API --------- */

void ui_power_screen_shutdown(uint32_t seconds) {
    // "Sistem Kapatiliyor" mesajında 'I' harfi yerine TR fontun varsa 0xDD (İ) de basabilirsin
    // Şimdilik string içindeki ASCII karakterlerle test et.
    power_anim("Sistem Kapatiliyor", seconds, 0);
}

void ui_power_screen_reboot(uint32_t seconds) {
    power_anim("Sistem Yeniden Baslatiliyor", seconds, 1);
}