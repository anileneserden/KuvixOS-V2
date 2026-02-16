#include <kernel/fs/vfs.h>
#include <kernel/fs/kvxfs.h> // BU SATIRI EKLEDİK
#include <kernel/printk.h>
#include <lib/commands.h>

void cmd_mkdir(int argc, char** argv) {
    if (argc < 2) {
        printk("Kullanım: mkdir <dizin_adi>\n");
        return;
    }

    // Doğrudan KVXFS'i çağırıyoruz
    if (kvxfs_mkdir(argv[1]) == 0) {
        printk("Dizin oluşturuldu: %s\n", argv[1]);
    } else {
        printk("Hata: Dizin oluşturulamadı (KVXFS Hatası): %s\n", argv[1]);
    }
}
REGISTER_COMMAND(mkdir, cmd_mkdir, "Yeni bir dizin oluşturur");