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
    ensure_dir("/home");
    ensure_dir(USER_HOME_PATH);
    ensure_dir(USER_DESKTOP_PATH);
    ensure_dir(USER_APPS_PATH);
    ensure_dir(USER_DOWNLOAD_PATH);
    ensure_dir(USER_TRASH_PATH);
    ensure_dir(USER_HTML_PATH);
    ensure_dir(USER_HTML_TEST_PATH);

    printk("[FS] user layout ok: %s\n", USER_HOME_PATH);
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

    // KVXFS'i başlatmayı dene
    if (kvxfs_init()) {
        printk("KVXFS: Disk sistemi basariyla baglandi.\n");
    } else {
        // BAĞLANAMADIYSA: Muhtemelen disk boş veya magic hatalı.
        printk("KVXFS: Magic hatasi! Disk otomatik formatlaniyor...\n");
        
        if (kvxfs_format()) {
            printk("KVXFS: Disk basariyla formatlandi ve hazir.\n");
            // Format sonrası tekrar init et ki g_inited = 1 olsun
            kvxfs_init(); 
        } else {
            printk("KVXFS: KRITIK HATA! Disk formatlanamadi.\n");
        }
    }

    fs_prepare_user_layout();

    return 1;
}