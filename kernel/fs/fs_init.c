#include <kernel/fs/vfs.h>
#include <kernel/fs/kvxfs.h>
#include <kernel/fs/toyfs.h>
#include <kernel/drivers/ata_pio.h>
#include <kernel/block/block.h>
#include <kernel/printk.h>

int fs_init_once(void) {
    vfs_init();

    if (ata_pio_init()) {
        blockdev_t* dev = ata_pio_get_dev();
        if (dev) {
            block_set_root(dev);
        }
    }

    // KVXFS'i başlatmayı dene
    if (kvxfs_init()) {
        printk("KVXFS: Disk sistemi basariyla baglandi.\n");
    } else {
        // Disk bagli ama formatlı değilse kullanıcıya bildir
        printk("KVXFS: Kalici disk formatli degil! 'format' komutunu kullanin.\n");
    }

    return 1;
}