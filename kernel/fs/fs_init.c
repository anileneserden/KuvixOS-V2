#include <kernel/fs/vfs.h>
#include <kernel/fs/kvxfs.h>
#include <kernel/fs/toyfs.h>
#include <kernel/drivers/ata_pio.h>
#include <kernel/block/block.h>
#include <kernel/printk.h>

int fs_init_once(void) {
    // 1. VFS Temel Yapısını Hazırla (RAMFS burada başlar)
    vfs_init();

    // 2. ATA/IDE Sürücüsünü Başlat (disk.img'i görmesi için)
    // Not: ata_init() fonksiyonunun adını kendi sürücüne göre kontrol et
    ata_pio_init(); 

    // 3. Blok katmanına root cihazını tanıt (disk.img genellikle ilk disk)
    // block_set_root(dev) gibi bir fonksiyonun varsa kullan, 
    // yoksa KVXFS doğrudan block_read/write kullanacaktır.

    // 4. KVXFS'i Başlat (/persist/ katmanı)
    if (kvxfs_init()) {
        printk("KVXFS: Disk sistemi basariyla baglandi.\n");
    } else {
        printk("KVXFS: Kalici disk bulunamadi veya formatli degil.\n");
    }

    // 5. ToyFS'i Başlat (Eğer blok cihazı üzerinden çalışıyorsa)
    // toyfs_init(); 

    return 1; // Hata verse bile shell açılsın diye 1 dönüyoruz
}