#include <kernel/fs/vfs.h>
#include <kernel/fs/kvxfs.h>
#include <kernel/fs/toyfs.h>
#include <kernel/drivers/ata_pio.h>
#include <kernel/block/block.h>
#include <kernel/printk.h>
#include <kernel/serial.h>
#include <kernel/user.h>
#include <lib/string.h>

static void ensure_dir(const char* p) {
    if (!p || !p[0]) return;
    vfs_mkdir(p);
}

int fs_prepare_user_layout(void) {
    // Disk üzerinde olması gereken temel dizinleri kontrol edip oluşturuyoruz
    ensure_dir("/home");
    ensure_dir("/home/anil");
    ensure_dir("/sys");
    ensure_dir("/sys/drivers");
    ensure_dir("/etc");

    // /etc/passwd dosyası daha önce oluşturulmadıysa ilk açılışta yazıyoruz
    if (!vfs_exists("/etc/passwd")) {
        const char* passwd_data = "root:x:0:0:root:/root:/bin/sh\nanil:x:1000:1000:Anil:/home/anil:/bin/sh\n";
        // kvxfs_write_all kullanarak dosyayı içeriğiyle birlikte diske kaydediyoruz
        int res = kvxfs_write_all("/etc/passwd", (const uint8_t*)passwd_data, strlen(passwd_data));
        if (res) {
            printk("[FS] /etc/passwd basariyla olusturuldu.\n");
        } else {
            printk("[FS] HATA: /etc/passwd olusturulamadi!\n");
        }
    }

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