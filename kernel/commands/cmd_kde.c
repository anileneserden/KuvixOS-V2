#include <kernel/drivers/video/de_api.h>
#include <kernel/drivers/video/fb_console.h>
#include <kernel/drivers/video/fb.h>
#include <kernel/fs/vfs.h>
#include <kernel/memory/kmalloc.h>
#include <lib/commands.h>
#include <lib/string.h>
#include <kernel/printk.h>
#include <kernel/drivers/video/gfx.h>

// Dikdörtgen çizim köprüsü
void kernel_draw_rect(int start_x, int start_y, int w, int h, uint32_t color) {
    for (int y = start_y; y < start_y + h; y++) {
        for (int x = start_x; x < start_x + w; x++) {
            fb_putpixel(x, y, color);
        }
    }
}

// Hatayı çözen yazı çizim köprüsü (Wrapper)
// DE_API'den (x, y, text, color) alır, kernel'a (x, y, color, text) olarak iletir.
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
        commands_puts("Error: Could not read /sys/de/desktop.kde. Make sure the file exists.\n");
        kfree(kde_buffer);
        return;
    }

    int width = fb_get_width();
    int height = fb_get_height();

    DE_API api;
    api.screen_width   = width;
    api.screen_height  = height;
    api.clear          = fb_clear;
    api.put_pixel      = fb_putpixel;

    api.draw_rect      = kernel_draw_rect;
    api.draw_text      = kernel_draw_text; 

    api.update_display = fb_present;
    api.log            = (void(*)(const char*))printk;

    fb_console_set_enabled(false);

    typedef void (*kde_entry_t)(DE_API*);
    kde_entry_t start_desktop = (kde_entry_t)kde_buffer;

    start_desktop(&api);

    fb_console_set_enabled(true);
    commands_puts("\n[KDE LOADER V2] Warning: Desktop execution finished. Returned to shell.\n");

    kfree(kde_buffer);
}

REGISTER_COMMAND(kde, cmd_kde, "Starts KuvixOS DEDK V2 Desktop Environment");