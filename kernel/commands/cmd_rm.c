#include <kernel/fs.h>
#include <kernel/printk.h>
#include <lib/string.h>
#include <lib/commands.h>

// vfs.c içindeki fonksiyonu dışarıdan alıyoruz
extern void vfs_remove_node(fs_node_t *parent, char *name);

void cmd_rm(int argc, char** argv) {
    if (argc < 2) {
        printk("Kullanim: rm <dosya_veya_dizin_adi>\n");
        return;
    }

    // Mevcut dizinden (fs_current) sil
    vfs_remove_node(fs_current, argv[1]);
}

REGISTER_COMMAND(rm, cmd_rm, "Bir dosya veya dizini siler");