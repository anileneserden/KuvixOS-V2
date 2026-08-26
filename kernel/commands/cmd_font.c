// lib/commands/cmd_font.c
#include <kernel/fs/vfs.h>
#include <lib/commands.h>
#include <kernel/memory/kmalloc.h>
#include <lib/string.h>
#include <kernel/printk.h>
#include <kernel/drivers/video/ttf.h>
#include <kernel/drivers/video/fb_console.h>

void cmd_font(int argc, char** argv) {
    if (argc < 2) {
        printk("Kullanım: font <ttf_dosya_yolu> [boyut_px]\n");
        printk("Örnek: font /sys/fonts/arial.ttf\n");
        printk("Örnek: font fontlar/custom.ttf 18\n");
        return;
    }

    const char* path = argv[1];
    float pixel_height = 16.0f; // Varsayılan boyut

    if (argc >= 3) {
        // Basit bir atoi fonksiyonu ile string boyutu floata çevrilebilir
        // (Eğer projende atoi yoksa doğrudan sayısal dönüştürme kullanabilirsin)
        int size_val = 0;
        const char* p = argv[2];
        while (*p >= '0' && *p <= '9') {
            size_val = size_val * 10 + (*p - '0');
            p++;
        }
        if (size_val > 0) {
            pixel_height = (float)size_val;
        }
    }

    char target_path[VFS_PATH_MAX] = {0};

    // Yol birleştirme işlemleri (cmd_cat ile aynı mantık)
    if (path[0] == '/') {
        strncpy(target_path, path, VFS_PATH_MAX - 1);
    } else {
        const char* current_cwd = vfs_get_cwd();
        if (!current_cwd) current_cwd = "/";
        strncpy(target_path, current_cwd, VFS_PATH_MAX - 1);
        size_t len = strlen(target_path);
        if (len > 0 && target_path[len - 1] != '/') {
            strcat(target_path, "/");
        }
        strcat(target_path, path);
    }

    printk("[FONT] Yukleniyor: %s (Boyut: %.1fpx)...\n", target_path, pixel_height);

    // 1. Dosya varlık kontrolü (vfs_stat)
    vfs_stat_t st;
    if (!vfs_stat(target_path, &st)) {
        printk("Hata: Font dosyasi bulunamadi: %s\n", target_path);
        return;
    }

    // 2. VFS üzerinden dosyayı aç
    vfs_file_t* file = NULL;
    if (!vfs_open(target_path, 0, &file)) {
        printk("Hata: Font dosyasi acilamadi veya yetki yok: %s\n", target_path);
        return;
    }

    // 3. Bellek alanı ayır (TrueType dosyaları genellikle 50KB - 500KB arasındadır)
    uint32_t max_size = 512 * 1024; // 512 KB tampon
    uint8_t* font_buf = kmalloc(max_size);
    if (!font_buf) {
        printk("Hata: Font yüklemek için yeterli bellek ayrılamadı!\n");
        // vfs_close(file);
        return;
    }

    // 4. Dosyayı belleğe oku
    uint32_t bytes_read = 0;
    int read_res = vfs_read(file, font_buf, max_size, &bytes_read);
    // vfs_close(file);

    if (!read_res || bytes_read == 0) {
        printk("Hata: Font dosyasi okunamadi: %s\n", target_path);
        kfree(font_buf);
        return;
    }

    // 5. TTF motoruna yeni fontu aktar
    if (ttf_init_from_memory(font_buf, bytes_read, pixel_height)) {
        printk("Basarili: Sistem fontu guncellendi!\n");
        // Ekranda anında değişimin hissedilmesi için konsolu temizleyebiliriz
        fb_console_clear();
    } else {
        printk("Hata: Gecersiz veya bozuk TrueType (.ttf) formati!\n");
        kfree(font_buf); // Başarısız olursa belleği geri ver
    }
}

REGISTER_COMMAND(font, cmd_font, "Changes the active system TTF font");