#include <kernel/drivers/video/de_api.h>
#include <kernel/drivers/video/fb_console.h>
#include <kernel/drivers/video/fb.h>
#include <kernel/fs/vfs.h>
#include <kernel/memory/kmalloc.h>
#include <lib/commands.h>
#include <lib/string.h>
#include <kernel/printk.h>

void cmd_kde(int argc, char** argv) {
    (void)argc; (void)argv;

    commands_puts("[KDE LOADER V2] /home/anil/desktop.kde yukleniyor...\n");

    // 1. Masaüstü binary'si için üst sınır bellek alanı tanımlıyoruz (64 KB)
    uint32_t max_size = 64 * 1024; 
    uint8_t* kde_buffer = (uint8_t*)kmalloc(max_size);

    if (!kde_buffer) {
        commands_puts("Error: Memory allocation failed for KDE buffer.\n");
        return;
    }

    uint32_t nread = 0;

    // 2. vfs_read_all fonksiyonuyla ham binary'yi doğrudan belleğe çekiyoruz
    if (vfs_read_all("/home/anil/desktop.kde", kde_buffer, max_size, &nread)) {
        if (nread == 0) {
            commands_puts("Error: /home/anil/desktop.kde is empty!\n");
            kfree(kde_buffer);
            return;
        }
    } else {
        commands_puts("Error: Could not read /home/anil/desktop.kde. Make sure the file exists.\n");
        kfree(kde_buffer);
        return;
    }

    // 3. Grafik köprüsünü (DE_API) dolduruyoruz
    DE_API api;
    api.clear          = fb_clear;
    api.put_pixel      = fb_putpixel; // KuvixOS kernel fonksiyon adına göre eşitlendi
    api.update_display = fb_present;
    api.log            = (void(*)(const char*))printk; // printk köprüsü

    // 4. KST metin konsolunu kapatıyoruz (Yırtılmaları ve kaymaları önlemek için)
    fb_console_set_enabled(false);

    // 5. Zıplama Çizgisi (Function Pointer)
    // DEDK v2 tarafında _start fonksiyonunu tam 0x0 adresine bağladığımız için direkt tamponun başına zıplıyoruz.
    typedef void (*kde_entry_t)(DE_API*);
    kde_entry_t start_desktop = (kde_entry_t)kde_buffer;

    // KONTROLÜ RESMEN MASAÜSTÜNE DEVREDİYORUZ!
    start_desktop(&api);

    // 6. Güvenlik Duvarı
    // Masaüstü döngüsü bir şekilde biterse, sistem karanlıkta kalmasın diye konsolu geri açıyoruz.
    fb_console_set_enabled(true);
    commands_puts("\n[KDE LOADER V2] Warning: Desktop execution finished. Returned to shell.\n");

    // Belleği temizliyoruz
    kfree(kde_buffer);
}

// KuvixOS otomatik komut kayıt makrosu
REGISTER_COMMAND(kde, cmd_kde, "Starts KuvixOS DEDK V2 Desktop Environment");