#include <kernel/fs.h>
#include <kernel/printk.h>
#include <lib/string.h>
#include <lib/commands.h>

void cmd_cat(int argc, char** argv) {
    if (argc < 2) {
        printk("Kullanim: cat <dosya_adi>\n");
        return;
    }

    // Dosyayı isme göre ara
    fs_node_t *node = finddir_fs(fs_root, argv[1]);

    if (node) {
        if (node->flags & FS_DIRECTORY) {
            printk("Hata: '%s' bir dizindir.\n", argv[1]);
            return;
        }

        // Buffer ayır ve içeriği oku
        char buffer[512];
        memset(buffer, 0, 512);

        // VFS üzerinden veriyi oku
        uint32_t size = read_fs(node, 0, 511, (uint8_t*)buffer);

        if (size > 0) {
            printk("%s\n", buffer);
        } else {
            printk("(Dosya bos)\n");
        }
    } else {
        printk("Hata: '%s' dosyasi bulunamadi.\n", argv[1]);
    }
}

REGISTER_COMMAND(cat, cmd_cat, "Dosya icerigini ekrana basar");