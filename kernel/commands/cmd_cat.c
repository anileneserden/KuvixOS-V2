#include <kernel/fs/vfs.h>
#include <kernel/printk.h>
#include <lib/string.h>
#include <lib/commands.h>

void cmd_cat(int argc, char** argv) {
    if (argc < 2) {
        printk("Kullanim: cat <dosya>\n");
        return;
    }

    uint8_t buffer[4096];
    uint32_t size = 0;
    
    if (vfs_read_all(argv[1], buffer, sizeof(buffer), &size)) {
        for (uint32_t i = 0; i < size; i++) {
            printk("%c", buffer[i]);
        }
        printk("\n");
    } else {
        printk("Hata: Dosya okunamadi: %s\n", argv[1]);
    }
}

REGISTER_COMMAND(cat, cmd_cat, "Dosya içeriğini okur ve ekrana basar");