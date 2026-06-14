#include <kernel/fs/vfs.h>
#include <kernel/printk.h>
#include <lib/commands.h>
#include <lib/string.h>
#include <kernel/memory/kmalloc.h>

void cmd_parsejson(int argc, char** argv) {
    if (argc < 2) {
        commands_puts("Kullanım: parsejson <dosya.json>\n");
        return;
    }

    char* path = argv[1];
    uint32_t read_sz = 0;
    
    // 1. Dosyayı oku (1024 byte şimdilik yeterli test için)
    uint8_t buffer[1024];
    int ok = vfs_read_all(path, buffer, sizeof(buffer) - 1, &read_sz);
    
    if (ok <= 0) {
        commands_printf("Hata: Dosya bulunamadi veya okunamadi: %s\n", path);
        return;
    }
    buffer[read_sz] = '\0'; // String sonlandırıcı ekle

    char* ptr = (char*)buffer;
    commands_puts("--- Kuvix JSON Parser v0.1 ---\n");

    // 2. Basit Tarama Algoritması
    // JSON içindeki her { ... } bloğunu gezer
    while ((ptr = strstr(ptr, "{")) != NULL) {
        // "name" anahtarını bul
        char* name_key = strstr(ptr, "\"name\":");
        // "message" anahtarını bul
        char* msg_key = strstr(ptr, "\"message\":");

        if (name_key && msg_key) {
            // Değerlerin tırnak içindeki kısımlarını ayıkla
            char* name_start = strstr(name_key + 7, "\"") + 1;
            char* name_end = strstr(name_start, "\"");
            
            char* msg_start = strstr(msg_key + 10, "\"") + 1;
            char* msg_end = strstr(msg_start, "\"");

            if (name_end && msg_end) {
                // Geçici olarak null koyup ekrana basıyoruz
                *name_end = '\0';
                *msg_end = '\0';

                commands_printf("Kullanici: %s\n", name_start);
                commands_printf("Mesaj    : %s\n", msg_start);
                commands_puts("------------------------------\n");

                // Orijinal tırnakları geri koy (ilerlemeye devam etmek için)
                *name_end = '"';
                *msg_end = '"';
            }
        }
        ptr++; // Bir sonraki { için ilerle
    }
}

// ✅ Senin istediğin makro sistemiyle kayıt ediyoruz
REGISTER_COMMAND(parsejson, cmd_parsejson, "JSON dosyalarini ayristirir ve verileri listeler");