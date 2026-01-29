#include <lib/commands.h>
#include <kernel/printk.h>
#include <stddef.h>

// Linker Script içindeki sembolleri alıyoruz
extern command_t _cmd_start;
extern command_t _cmd_end;

// Kendi string karşılaştırma fonksiyonumuz (libc olmadığı için)
static int k_streq(const char* a, const char* b) {
    int i = 0;
    while (a[i] && b[i]) {
        if (a[i] != b[i]) return 0;
        i++;
    }
    return a[i] == 0 && b[i] == 0;
}

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
    char* argv[16]; // Maksimum 16 argüman desteği
    int argc = split_line(line, argv, 16);

    if (argc == 0) return; // Boş satır

    // Linker tarafından oluşturulan cmd_section tablosunda arama yapıyoruz
    command_t* cmd = &_cmd_start;
    
    // Pointer aritmetiği ile _cmd_start'tan _cmd_end'e kadar tüm command_t yapılarını gez
    for (; cmd < &_cmd_end; cmd++) {
        if (k_streq(argv[0], cmd->name)) {
            cmd->fn(argc, argv);
            return;
        }
    }

    printk("Bilinmeyen komut: '%s'. Yardim icin 'help' yazin.\n", argv[0]);
}