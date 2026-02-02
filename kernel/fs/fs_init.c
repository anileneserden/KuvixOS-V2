#include <kernel/fs/vfs.h>
#include <kernel/fs/kvxfs.h>
#include <kernel/fs/toyfs.h>
#include <kernel/drivers/ata_pio.h>
#include <kernel/block/block.h>
#include <kernel/printk.h>

int fs_init_once(void) {
    // 1. VFS Temel Yapısını Hazırla
    vfs_init();

    // 2. ATA/IDE Sürücüsünü Başlat
    if (ata_pio_init()) {
        // 3. Sürücü hazırsa, cihaz nesnesini al ve sisteme "Kök Cihaz" yap
        blockdev_t* dev = ata_pio_get_dev();
        if (dev) {
            block_set_root(dev); // BU SATIR EKSİKTİ: g_root artık dolu!
        }
    }

    // 4. KVXFS'i Başlat
    // g_root artık dolu olduğu için block_write() gerçek sürücüye gidecek.
    if (kvxfs_init()) {
        printk("KVXFS: Disk sistemi basariyla baglandi.\n");
    } else {
        printk("KVXFS: Kalici disk bulunamadi veya formatli degil.\n");
    }

    return 1;
}