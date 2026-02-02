#include <kernel/fs/vfs.h>
#include <kernel/fs/kvxfs.h> // BU SATIRI EKLEDİK
#include <kernel/printk.h>
#include <lib/commands.h>

void cmd_mkdir(int argc, char** argv) {
    if (argc < 2) {
        printk("Kullanim: mkdir <dizin_adi>\n");
        return;
    }

    // Doğrudan KVXFS'i çağırıyoruz
    if (kvxfs_mkdir(argv[1]) == 0) {
        printk("Dizin olusturuldu: %s\n", argv[1]);
    } else {
        printk("Hata: Dizin olusturulamadi (KVXFS Hatasi): %s\n", argv[1]);
    }
}
REGISTER_COMMAND(mkdir, cmd_mkdir, "Yeni bir dizin olusturur");