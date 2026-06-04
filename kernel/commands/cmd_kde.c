#include <kernel/drivers/video/de_api.h>
#include <kernel/drivers/video/fb_console.h>
#include <kernel/drivers/video/fb.h>
#include <kernel/fs/vfs.h>
#include <kernel/memory/kmalloc.h>
#include <lib/commands.h>
#include <lib/string.h>
#include <kernel/printk.h>

void cmd_kde(int argc, char** argv) {
    (void)argc; (void)argv;

    commands_puts("[KDE LOADER V2] /home/anil/desktop.kde yukleniyor...\n");

    uint32_t max_size = 64 * 1024; 
    uint8_t* kde_buffer = (uint8_t*)kmalloc(max_size);

    if (!kde_buffer) {
        commands_puts("Error: Memory allocation failed for KDE buffer.\n");
        return;
    }

    uint32_t nread = 0;

    if (vfs_read_all("/home/anil/desktop.kde", kde_buffer, max_size, &nread)) {
        if (nread == 0) {
            commands_puts("Error: /home/anil/desktop.kde is empty!\n");
            kfree(kde_buffer);
            return;
        }
    } else {
        commands_puts("Error: Could not read /home/anil/desktop.kde. Make sure the file exists.\n");
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