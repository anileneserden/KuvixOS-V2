#include <kernel/drivers/video/de_api.h>
#include <kernel/drivers/video/fb_console.h>
#include <kernel/drivers/video/fb.h>
#include <kernel/fs/vfs.h>
#include <kernel/memory/kmalloc.h>
#include <lib/commands.h>
#include <lib/string.h>
#include <kernel/printk.h>
#include <kernel/drivers/video/gfx.h>
#include <kernel/drivers/rtc/rtc.h>
#include <kernel/drivers/input/mouse_ps2.h>
#include <kernel/drivers/input/keyboard.h> // <-- Klavye sürücü başlığı eklendi

#define KDE_LOAD_ADDRESS 0x00800000

// --- API SARMALAYICILARI (WRAPPERS) ---

static void kernel_draw_rect(int x, int y, int w, int h, uint32_t color) {
    int max_w = fb_get_width();
    int max_h = fb_get_height();

    int end_x = x + w;
    int end_y = y + h;

    if (x < 0) x = 0;
    if (y < 0) y = 0;
    if (end_x > max_w) end_x = max_w;
    if (end_y > max_h) end_y = max_h;

    for (int cy = y; cy < end_y; cy++) {
        for (int cx = x; cx < end_x; cx++) {
            fb_putpixel(cx, cy, color);
        }
    }
}

static void kernel_draw_text(int x, int y, const char* text, uint32_t color) {
    if (!text || text[0] == '\0') return;
    gfx_draw_text_utf8(x, y, color, text);
}

static void kernel_get_mouse(de_mouse_state_t* state) {
    if (!state) return;

    extern void ps2_mouse_poll(void); 
    ps2_mouse_poll();
    ps2_mouse_update();

    // Dinamik ekran çözünürlüğü
    int max_w = (int)fb_get_width();
    int max_h = (int)fb_get_height();

    // Çözünürlüğe göre kısıtla
    if (mouse_x < 0) mouse_x = 0;
    if (mouse_y < 0) mouse_y = 0;
    if (mouse_x >= max_w) mouse_x = max_w - 1;
    if (mouse_y >= max_h) mouse_y = max_h - 1;

    state->x = mouse_x;
    state->y = mouse_y;
    state->left_button   = (g_mouse_buttons & 0x01) ? 1 : 0;
    state->right_button  = (g_mouse_buttons & 0x02) ? 1 : 0;
    state->middle_button = (g_mouse_buttons & 0x04) ? 1 : 0;
}

static char kernel_get_key(void) {
    kbd_poll();

    if (kbd_has_character()) {
        return kbd_get_char();
    }

    return 0;
}

static void kernel_get_time(char* buffer) {
    if (!buffer) return;
    rtc_datetime_t dt;
    if (rtc_read_datetime(&dt)) {
        buffer[0] = (dt.hour / 10) + '0'; buffer[1] = (dt.hour % 10) + '0';
        buffer[2] = ':';
        buffer[3] = (dt.min / 10) + '0';  buffer[4] = (dt.min % 10) + '0';
        buffer[5] = ':';
        buffer[6] = (dt.sec / 10) + '0';  buffer[7] = (dt.sec % 10) + '0';
        buffer[8] = '\0';
    } else {
        strcpy(buffer, "00:00:00");
    }
}

static void kernel_log(const char* msg) {
    if (msg) printk("%s", msg);
}

static DE_API g_kde_api;

void cmd_kde(int argc, char** argv) {
    (void)argc; (void)argv;

    printk("[KDE LOADER] /sys/de/desktop.kde yukleniyor...\n");

    uint32_t max_size = 64 * 1024;
    uint8_t* kde_target = (uint8_t*)kmalloc(max_size);

    if (!kde_target) {
        printk("Hata: Bellek ayrilamadi.\n");
        return;
    }

    memset(kde_target, 0, max_size);

    uint32_t nread = 0;
    if (!vfs_read_all("/sys/de/desktop.kde", kde_target, max_size, &nread) || nread == 0) {
        printk("Hata: /sys/de/desktop.kde okunamadi!\n");
        kfree(kde_target);
        return;
    }

    printk("[KDE LOADER] %d bayt %p adresine yuklendi.\n", nread, (void*)kde_target);

    memset(&g_kde_api, 0, sizeof(DE_API));
    g_kde_api.screen_width   = fb_get_width();
    g_kde_api.screen_height  = fb_get_height();
    g_kde_api.put_pixel      = fb_putpixel;
    g_kde_api.draw_rect      = kernel_draw_rect;
    g_kde_api.draw_text      = kernel_draw_text;
    g_kde_api.clear_screen   = fb_clear;
    g_kde_api.update_display = fb_present;
    g_kde_api.get_mouse      = kernel_get_mouse;
    g_kde_api.get_key        = kernel_get_key;
    g_kde_api.get_time       = kernel_get_time;
    g_kde_api.log            = kernel_log;

    printk("[KDE DUMP] Ilk 16 bayt: ");
    for(int i = 0; i < 16; i++) {
        printk("%x ", (unsigned int)kde_target[i]);
    }
    printk("\n");

    printk("[KDE LOADER] 'start_desktop' fonksiyon isaretcisine atlaniyor...\n");

    fb_console_set_enabled(false);
    fb_clear(0x000000);
    if (fb_present) {
        fb_present();
    }

    typedef void (__attribute__((cdecl)) *kde_entry_t)(DE_API*);
    kde_entry_t start_desktop = (kde_entry_t)(uintptr_t)kde_target;

    start_desktop(&g_kde_api);

    fb_console_set_enabled(true);
    printk("[KDE LOADER] 'desktop.kde' kapandi.\n");
    kfree(kde_target);
}

REGISTER_COMMAND(kde, cmd_kde, "Starts KuvixOS DEDK V2 Desktop Environment");