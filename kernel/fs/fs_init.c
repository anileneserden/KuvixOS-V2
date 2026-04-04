#include <kernel/fs/vfs.h>
#include <kernel/fs/kvxfs.h>
#include <kernel/fs/toyfs.h>
#include <kernel/drivers/ata_pio.h>
#include <kernel/drivers/usb/xhci.h>
#include <kernel/block/block.h>
#include <kernel/printk.h>
#include <kernel/serial.h>
#include <kernel/system/removable.h>

#include <kernel/user.h>   // ✅ eklendi

static void ensure_dir(const char* p) {
    if (!p || !p[0]) return;
    // vfs_mkdir "zaten varsa" hata dönse bile önemli değil.
    vfs_mkdir(p);
}

int fs_prepare_user_layout(void) {
    ensure_dir("/home");
    ensure_dir(USER_HOME_PATH);
    ensure_dir(USER_DESKTOP_PATH);
    ensure_dir(USER_APPS_PATH);
    ensure_dir(USER_TRASH_PATH);

    printk("[FS] user layout ok: %s\n", USER_HOME_PATH);
    return 1;
}

int fs_init_once(void) {
    // 1. VFS Temel Yapısını Hazırla
    vfs_init();
    printk("FS Init edildi\n");

    // 2. ATA/IDE Sürücüsünü Başlat
    if (ata_pio_init()) {
        // 3. Sürücü hazırsa, cihaz nesnesini al ve sisteme "Kök Cihaz" yap
        blockdev_t* dev = ata_pio_get_dev();
        if (dev) {
            block_set_root(dev);
        }
    }

    // 4. KVXFS'i Başlat
    if (kvxfs_init()) {
        printk("KVXFS: Disk sistemi basariyla baglandi.\n");
    } else {
        printk("KVXFS: Kalici disk bulunamadi veya formatli degil.\n");
    }

    g_removable_plugged = false;
    if (xhci_usb_msc_ready()) {
        blockdev_t* usb_dev = xhci_usb_msc_get_dev();
        if (usb_dev && toyfs_mount(usb_dev)) {
            g_removable_plugged = true;
            printk("[FS] USB removable mounted at /removable (ToyFS).\n");
        } else {
            printk("[FS] USB MSC hazır ama medya ToyFS degil veya mount basarisiz.\n");
        }
    }

    // ✅ 5. Kullanıcı dizinlerini hazırla (desktop değil kernel yapacak)
    fs_prepare_user_layout();

    return 1;
}