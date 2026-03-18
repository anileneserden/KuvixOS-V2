#include <kernel/printk.h>
#include <kernel/fs/vfs.h>
#include <lib/commands.h>
#include <lib/string.h>

static int ls_callback(const char* path, uint32_t size, void* u) {
    (void)u;
    if (size == 0xFFFFFFFF) {
        printk("[DIR]  %s\n", path);
    } else {
        // En basit formatı kullanalım
        printk("[FILE] %s  (%d byte)\n", path, (int)size);
    }
    return 1;
}

static void cmd_ls(int argc, char** argv) {
    char path[128];
    
    if (argc > 1) {
        strcpy(path, argv[1]);
    } else {
        // Eğer yol verilmediyse mevcut dizini (CWD) al
        strcpy(path, vfs_get_cwd());
    }

    printk("Dizin listeleniyor: %s\n", path);
    printk("------------------------------------\n");

    // vfs_list_dir yerine vfs_list kullanıyoruz ve callback veriyoruz
    if (!vfs_list(path, ls_callback, 0)) {
        printk("Hata: Dizin listelenemedi!\n");
    }
    
    printk("------------------------------------\n");
}

REGISTER_COMMAND(ls, cmd_ls, "Dizin icerigini listeler.");
REGISTER_COMMAND(dir, cmd_ls, "Dizin icerigini listeler.");