#include <kernel/fs/vfs.h>
#include <lib/commands.h>
#include <kernel/memory/kmalloc.h>
#include <lib/string.h>

void cmd_cat(int argc, char** argv) {
    if (argc < 2) {
        commands_puts("Usage: cat <file>\n");
        return;
    }

    // Statik bir buffer kullanalım (test için en güvenlisi)
    static uint8_t cat_buf[4096]; 
    uint32_t nread = 0;

    // vfs_read_all zaten /home ve /persist yollarını akıllıca çözüyor
    if (vfs_read_all(argv[1], cat_buf, sizeof(cat_buf) - 1, &nread)) {
        if (nread > 0) {
            cat_buf[nread] = '\0'; // String sonu garantisi
            commands_puts((const char*)cat_buf);
            commands_puts("\n");
        } else {
            commands_puts("(File is empty)\n");
        }
    } else {
        commands_puts("Error: Could not read file.\n");
    }
}

REGISTER_COMMAND(cat, cmd_cat, "Displays file content");