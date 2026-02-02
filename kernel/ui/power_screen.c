#include <ui/power_screen.h>

#include <kernel/drivers/video/fb.h>
#include <font/font8x8_basic.h>
#include <kernel/power.h>
#include <stdint.h>

/* --------- küçük yardımcılar --------- */
static void draw_text8(int x, int y, uint32_t color, const char* s)
{
    while (*s) {
        uint8_t c = (uint8_t)*s++;
        const uint8_t* glyph = font8x8_basic[c];
        for (int row = 0; row < 8; row++) {
            uint8_t line = glyph[row];
            for (int col = 0; col < 8; col++) {
                if (line & (1u << (7 - col))) fb_putpixel(x + col, y + row, color);
            }
        }
        x += 8;
    }
}

static int kstrlen(const char* s){ int n=0; while(s && s[n]) n++; return n; }

// çok basit busy-wait (interrupt gerekmez)
static void sleep_ms_busy(uint32_t ms)
{
    // QEMU’da hızlıysa iç döngüyü büyütebilirsin (ör: 600000)
    while (ms--) {
        for (volatile uint32_t i = 0; i < 60000; i++) {
            __asm__ __volatile__("pause");
        }
    }
}

static void power_anim(const char* msg, uint32_t seconds, int do_reboot)
{
    const char spin[4] = {'|','/','-','\\'};

    int sw = (int)fb_get_width();
    int sh = (int)fb_get_height();

    int msg_w = kstrlen(msg) * 8;
    int x_msg = (sw - msg_w) / 2;
    int y_msg = sh/2 - 20;

    for (int t = (int)seconds; t > 0; t--) {
        for (int f = 0; f < 10; f++) {
            fb_clear(fb_rgb(0,0,0));

            draw_text8(x_msg, y_msg, fb_rgb(255,255,255), msg);

            char cnt[6];
            cnt[0] = (char)('0' + t);  // 1..9 için yeter
            cnt[1] = '.';
            cnt[2] = '.';
            cnt[3] = '.';
            cnt[4] = 0;

            int x_cnt = (sw - 4*8) / 2;
            int y_cnt = y_msg + 16;
            draw_text8(x_cnt, y_cnt, fb_rgb(200,200,200), cnt);

            char sp[2] = { spin[(t*10 + f) & 3], 0 };
            int x_sp = (sw / 2) - 4;
            int y_sp = y_cnt + 16;
            draw_text8(x_sp, y_sp, fb_rgb(200,200,200), sp);

            fb_present();
            sleep_ms_busy(10);
        }
    }

    // Son frame
    fb_clear(fb_rgb(0,0,0));
    draw_text8(x_msg, y_msg, fb_rgb(255,255,255), msg);
    fb_present();
    sleep_ms_busy(200);

    // BURADA artık sistemi kapat/reboot et
    if (do_reboot) power_reboot();
    else           power_shutdown();
}

/* --------- dış API --------- */
void ui_power_screen_shutdown(uint32_t seconds)
{
    power_anim("Sistem Kapatiliyor", seconds, 0);
}

void ui_power_screen_reboot(uint32_t seconds)
{
    power_anim("Sistem Yeniden Baslatiliyor", seconds, 1);
}
