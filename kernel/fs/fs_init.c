#include <kernel/fs/vfs.h>
#include <kernel/fs/kvxfs.h>
#include <kernel/fs/toyfs.h>
#include <kernel/drivers/ata_pio.h>
#include <kernel/block/block.h>
#include <kernel/printk.h>
#include <kernel/serial.h>

#include <kernel/user.h>

static void ensure_dir(const char* p) {
    if (!p || !p[0]) return;
    vfs_mkdir(p);
}

int fs_prepare_user_layout(void) {
    // ensure_dir uyarısını gidermek için diskte olması gereken yerleri kontrol edelim
    // Eğer yoklarsa (disk yeniyse) oluşturur
    ensure_dir("/home");
    ensure_dir("/home/anil");
    ensure_dir("/sys");
    ensure_dir("/sys/themes");

    if (vfs_exists("/home/anil")) {
        printk("[FS] Kullanici dizini hazir: /home/anil\n");
    } else {
        printk("[FS] HATA: Kullanici dizini olusturulamadi!\n");
    }
    return 1;
}

int fs_init_once(void) {
    vfs_init();
    printk("FS Init edildi\n");

    if (ata_pio_init()) {
        blockdev_t* dev = ata_pio_get_dev();
        if (dev) {
            block_set_root(dev);
        }
    }

    if (kvxfs_init()) {
        printk("KVXFS: Disk sistemi basariyla baglandi.\n");
        
        // 1. DURUM: Disk zaten formatlıydı ve bağlandı. Ağacı döküyoruz!
        kvxfs_tree("/"); 
        
    } else {
        printk("KVXFS: Kalici disk bulunamadi veya formatli degil. Format atiliyor...\n");
        if (kvxfs_force_format()) {
            printk("KVXFS: Format tamam.\n");
            if (kvxfs_init()) {
                printk("KVXFS: Disk sistemi basariyla baglandi.\n");
                
                // 2. DURUM: Disk yeni formatlandı ve bağlandı. Yeni ağacı döküyoruz!
                kvxfs_tree("/"); 
                
            } else {
                printk("KVXFS: format sonrasi init basarisiz.\n");
            }
        } else {
            printk("KVXFS: format basarisiz.\n");
        }
    }

    // Disk ağacını konsola bastıktan sonra kullanıcı katmanı dizinleri/dosyaları hazırlanıyor
    fs_prepare_user_layout();

    return 1;
}