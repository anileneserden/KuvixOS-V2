#include <lib/commands.h>
#include <kernel/printk.h>
#include <lib/shell.h>
#include <lib/string.h>

void cmd_cd(int argc, char** argv) {
    // 1. Parametre yoksa veya '~' ise ev dizinine git
    if (argc < 2 || (strcmp(argv[1], "~") == 0)) {
        shell_set_cwd("/home/root");
        return;
    }

    char* target = argv[1];
    char temp_path[128];
    char final_path[128];
    const char* current = shell_get_cwd();

    // 2. Başlangıç yolunu hazırla (Absolute mu Relative mi?)
    if (target[0] == '/') {
        // Mutlak yol: Doğrudan hedefi kopyala
        strncpy(temp_path, target, sizeof(temp_path) - 1);
    } else {
        // Bağıl yol: Mevcut dizinin üzerine ekle
        strcpy(temp_path, current);
        size_t len = strlen(temp_path);
        
        // Mevcut dizinin sonunda '/' yoksa ekle (Kök dizin "/" değilse)
        if (len > 0 && temp_path[len - 1] != '/') {
            strcat(temp_path, "/");
        }
        strcat(temp_path, target);
    }

    // 3. Yol Normalleştirme (.. ve . işlemleri)
    if (strcmp(target, "..") == 0 || strcmp(target, "../") == 0) {
        if (strcmp(current, "/") == 0) return; // Zaten kökteyiz

        strcpy(final_path, current);
        int len = strlen(final_path);
        
        // Sondaki eğik çizgiyi temizle (Örn: /home/ -> /home)
        if (len > 1 && final_path[len-1] == '/') final_path[--len] = '\0';

        // Sondan geriye doğru ilk '/' bulana kadar sil
        while (len > 0 && final_path[len-1] != '/') {
            len--;
        }

        // Eğer köke ulaştıysak '/' yap, yoksa son '/' işaretini temizle
        if (len <= 1) {
            strcpy(final_path, "/");
        } else {
            final_path[len-1] = '\0';
        }
        shell_set_cwd(final_path);
        return;
    }

    // 4. "." (Mevcut dizin) ise hiçbir şey yapma
    if (strcmp(target, ".") == 0 || strcmp(target, "./") == 0) {
        return;
    }

    // 5. SON TEMİZLİK: Kullanıcının girdiği "home/" gibi yollardaki 
    // sondaki fazla '/' işaretini temizle.
    size_t final_len = strlen(temp_path);
    if (final_len > 1 && temp_path[final_len - 1] == '/') {
        temp_path[final_len - 1] = '\0';
    }

    // 6. Temizlenmiş yolu sisteme kaydet
    shell_set_cwd(temp_path);
}

REGISTER_COMMAND(cd, cmd_cd, "Dizin degistirir ve yolu temizler");