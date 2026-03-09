#include <kernel/fs/vfs.h>
#include <kernel/fs/kvxfs.h>
#include <kernel/fs/toyfs.h>
#include <kernel/drivers/ata_pio.h>
#include <kernel/block/block.h>
#include <kernel/printk.h>
#include <kernel/serial.h>
#include <lib/string.h>

#include <kernel/user.h>   // ✅ eklendi

static void ensure_dir(const char* p) {
    if (!p || !p[0]) return;
    // vfs_mkdir "zaten varsa" hata dönse bile önemli değil.
    vfs_mkdir(p);
}

int fs_prepare_user_layout(void) {
    char home[128];
    char desktop[160];
    char apps[160];
    char trash[160];

    ensure_dir("/home");

    strncpy(home, user_get_home(), sizeof(home) - 1);
    home[sizeof(home) - 1] = '\0';

    user_get_desktop_path(desktop, sizeof(desktop));
    user_get_apps_path(apps, sizeof(apps));
    user_get_trash_path(trash, sizeof(trash));

    ensure_dir(home);
    ensure_dir(desktop);
    ensure_dir(apps);
    ensure_dir(trash);

    printk("[FS] user layout ok: %s\n", home);
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

    // ✅ 5. Kullanıcı dizinlerini hazırla (desktop değil kernel yapacak)
    fs_prepare_user_layout();

    return 1;
}