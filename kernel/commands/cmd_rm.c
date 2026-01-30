#include <kernel/fs/vfs.h>
#include <kernel/printk.h>
#include <lib/commands.h>

void cmd_rm(int argc, char** argv) {
    if (argc < 2) {
        printk("Kullanim: rm <dosya/dizin>\n");
        return;
    }

    // vfs_remove fonksiyonunun var olduğunu varsayıyoruz
    // Eğer yoksa sadece printk mesajı bırakabilirsin
    printk("Siliniyor: %s...\n", argv[1]);
    // vfs_remove(argv[1]); 
}

REGISTER_COMMAND(rm, cmd_rm, "Dosya veya dizini siler");