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

    int rc = vfs_mkdir(p);
    printk("[FS] mkdir %s -> %d\n", p, rc);

    vfs_stat_t st;
    int ok = vfs_stat(p, &st);
    printk("[FS] stat  %s -> %d type=%d\n", p, ok, st.type);
}

int fs_prepare_user_layout(void) {
    ensure_dir("/persist/home");
    ensure_dir("/persist/home/anil");
    ensure_dir("/persist/home/anil/desktop");
    ensure_dir("/persist/home/anil/apps");
    ensure_dir("/persist/home/anil/trash");

    printk("[FS] persist user layout ok: /persist/home/anil\n");
    return 1;
}

int fs_init_once(void) {
    // 1) VFS
    vfs_init();
    printk("[FS] VFS init edildi\n");

    // 2) ATA
    if (!ata_pio_init()) {
        printk("[FS] ATA init basarisiz\n");
        return 0;
    }

    // 3) block root
    blockdev_t* dev = ata_pio_get_dev();
    if (!dev) {
        printk("[FS] ATA device yok\n");
        return 0;
    }

    block_set_root(dev);
    printk("[FS] block root set edildi\n");

    // 4) KVXFS init
    if (!kvxfs_init()) {
        printk("[FS] KVXFS init basarisiz, format deneniyor...\n");

        // 5) auto format
        if (!kvxfs_force_format()) {
            printk("[FS] KVXFS format basarisiz\n");
            return 0;
        }

        printk("[FS] KVXFS format OK, tekrar init deneniyor...\n");

        // 6) tekrar init
        if (!kvxfs_init()) {
            printk("[FS] KVXFS format sonrasi init yine basarisiz\n");
            return 0;
        }
    }

    printk("[FS] KVXFS hazir\n");

    // 7) persist user tree
    if (!fs_prepare_user_layout()) {
        printk("[FS] user layout hazirlanamadi\n");
        return 0;
    }

    printk("[FS] fs_init_once tamam\n");
    return 1;
}