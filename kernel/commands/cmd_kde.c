#include <kernel/drivers/video/de_api.h>
#include <kernel/drivers/video/fb_console.h>
#include <kernel/drivers/video/fb.h>
#include <kernel/fs/vfs.h>
#include <kernel/memory/kmalloc.h>
#include <lib/commands.h>
#include <lib/string.h>
#include <kernel/printk.h>
#include <kernel/drivers/video/gfx.h>
#include <kernel/drivers/rtc/rtc.h> // RTC başlık dosyan

// Saati string formatında DEDK V2 için hazırlar
void kernel_get_time(char* buffer) {
    rtc_datetime_t dt;
    if (rtc_read_datetime(&dt)) {
        // Basit string formatlama (sprintf veya kütüphanen varsa onu kullan)
        buffer[0] = (dt.hour / 10) + '0'; buffer[1] = (dt.hour % 10) + '0';
        buffer[2] = ':';
        buffer[3] = (dt.min / 10) + '0'; buffer[4] = (dt.min % 10) + '0';
        buffer[5] = ':';
        buffer[6] = (dt.sec / 10) + '0'; buffer[7] = (dt.sec % 10) + '0';
        buffer[8] = '\0';
    } else {
        buffer[0] = '-'; buffer[1] = '-'; buffer[2] = ':'; buffer[3] = '-'; buffer[4] = '-'; buffer[5] = '\0';
    }
}

void kernel_draw_rect(int start_x, int start_y, int w, int h, uint32_t color) {
    for (int y = start_y; y < start_y + h; y++) {
        for (int x = start_x; x < start_x + w; x++) {
            fb_putpixel(x, y, color);
        }
    }
}

void kernel_draw_text(int x, int y, const char* text, uint32_t color) {
    gfx_draw_text_utf8(x, y, color, text);
}

void cmd_kde(int argc, char** argv) {
    (void)argc; (void)argv;

    commands_puts("[KDE LOADER V2] /sys/de/desktop.kde yukleniyor...\n");

    uint32_t max_size = 64 * 1024; 
    uint8_t* kde_buffer = (uint8_t*)kmalloc(max_size);

    if (!kde_buffer) {
        commands_puts("Error: Memory allocation failed for KDE buffer.\n");
        return;
    }

    uint32_t nread = 0;
    if (vfs_read_all("/sys/de/desktop.kde", kde_buffer, max_size, &nread)) {
        if (nread == 0) {
            commands_puts("Error: /sys/de/desktop.kde is empty!\n");
            kfree(kde_buffer);
            return;
        }
    } else {
        commands_puts("Error: Could not read /sys/de/desktop.kde.\n");
        kfree(kde_buffer);
        return;
    }

    // API Yapılandırması
    DE_API api;
    api.screen_width   = fb_get_width();
    api.screen_height  = fb_get_height();
    api.clear          = fb_clear;
    api.put_pixel      = fb_putpixel;
    api.draw_rect      = kernel_draw_rect;
    api.draw_text      = kernel_draw_text; 
    api.update_display = fb_present;
    api.log            = (void(*)(const char*))printk;
    
    // Yeni eklenen RTC köprüsü
    api.get_time       = kernel_get_time; 

    fb_console_set_enabled(false);

    typedef void (*kde_entry_t)(DE_API*);
    kde_entry_t start_desktop = (kde_entry_t)kde_buffer;

    start_desktop(&api);

    fb_console_set_enabled(true);
    kfree(kde_buffer);
}

REGISTER_COMMAND(kde, cmd_kde, "Starts KuvixOS DEDK V2 Desktop Environment");