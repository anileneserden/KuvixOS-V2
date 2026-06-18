#include <kernel/fs/vfs.h>
#include <lib/commands.h>
#include <kernel/memory/kmalloc.h>
#include <lib/string.h>
#include <kernel/printk.h>
#include <kernel/fs/kvxfs.h>

void cmd_cat(int argc, char** argv) {
    if (argc < 2) {
        printk("Kullanım: cat <dosya_yolu>\n");
        return;
    }

    const char* path = argv[1];
    
    // Geçici bir okuma tamponu (Buffer) oluşturuyoruz (Maksimum 4KB)
    static uint8_t cat_buf[4096]; 
    uint32_t read_size = 0;

    // Senin yazdığın kvxfs_read_all fonksiyonunu tetikliyoruz
    if (kvxfs_read_all(path, cat_buf, sizeof(cat_buf) - 1, &read_size)) {
        cat_buf[read_size] = '\0'; // String sonlandırıcı ekle
        printk("%s\n", (const char*)cat_buf);
    } else {
        printk("Hata: Dosya okunamadı veya bulunamadı!\n");
    }
}

REGISTER_COMMAND(cat, cmd_cat, "Displays file content");