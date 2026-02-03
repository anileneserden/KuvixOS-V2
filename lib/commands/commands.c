#include <lib/commands.h>
#include <kernel/printk.h>
#include <stddef.h>
#include <lib/string.h>
#include <stdbool.h>

// Linker Script içindeki sembolleri alıyoruz
extern command_t _cmd_start;
extern command_t _cmd_end;

// Satırı boşluklara göre argümanlara böler (echo merhaba dunya -> argc=3)
static int split_line(char* line, char** argv, int max_args) {
    int argc = 0;
    char* p = line;

    while (*p && argc < max_args) {
        // Baştaki boşlukları atla
        while (*p == ' ') p++;
        if (!*p) break;

        argv[argc++] = p;

        // Kelimenin sonuna kadar ilerle
        while (*p && *p != ' ') p++;

        // Boşluk bulduysak kelimeyi bitir ve bir sonrakine geç
        if (*p == ' ') {
            *p = '\0';
            p++;
        }
    }
    return argc;
}

void commands_execute(char* line) {
    // 1. ADIM: Girdi Kontrolü
    if (!line || strlen(line) == 0) return;

    // Sondaki görünmez karakterleri (ENTER, Boşluk vs.) temizle
    int len = strlen(line);
    while (len > 0 && (line[len-1] == '\n' || line[len-1] == '\r' || line[len-1] == ' ')) {
        line[--len] = '\0';
    }

    char* argv[16];
    int argc = split_line(line, argv, 16);
    if (argc == 0) return;

    // 2. ADIM: Linker Check (Sistem neden çalışmıyor?)
    command_t* start = &_cmd_start;
    command_t* end = &_cmd_end;
    uint32_t count = end - start;

    if (count == 0 || count > 1000) { // Mantıksız bir sayı varsa
        printk("\n[SISTEM HATASI] Komut tablosu yuklenemedi!\n");
        printk("Linker Sembolleri: Start: 0x%x, End: 0x%x\n", (uint32_t)start, (uint32_t)end);
        return;
    }

    // 3. ADIM: Arama ve Eşleşme
    command_t* cmd = start;
    bool found = false;

    for (uint32_t i = 0; i < count; i++) {
        // Debug için her komutu seri porta bas (Sadece sorunu çözene kadar)
        // printk("Deneniyor: %s == %s\n", argv[0], cmd[i].name);

        if (strcmp(argv[0], cmd[i].name) == 0) {
            cmd[i].fn(argc, argv);
            found = true;
            break;
        }
    }

    // 4. ADIM: Hata Bildirimi
    if (!found) {
        printk("\n[!] Komut bulunamadi: '%s'\n", argv[0]);
        printk("Kayitli toplam komut sayisi: %d\n", count);
    }
}