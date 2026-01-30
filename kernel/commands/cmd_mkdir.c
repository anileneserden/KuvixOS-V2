#include <kernel/fs/vfs.h>
#include <kernel/printk.h>
#include <lib/commands.h>

void cmd_mkdir(int argc, char** argv) {
    if (argc < 2) {
        printk("Kullanim: mkdir <dizin_adi>\n");
        return;
    }

    // Yeni VFS fonksiyonunu kullanıyoruz
    if (vfs_mkdir(argv[1]) == 0) {
        printk("Dizin olusturuldu: %s\n", argv[1]);
    } else {
        printk("Hata: Dizin olusturulamadi: %s\n", argv[1]);
    }
}
REGISTER_COMMAND(mkdir, cmd_mkdir, "Yeni bir dizin olusturur");