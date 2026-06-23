#include <kernel/fs/vfs.h>
#include <lib/commands.h>
#include <lib/string.h>
#include <kernel/fs/kvxfs.h>
#include <kernel/printk.h>

// Yardımcı Fonksiyon: Tam yoldan sadece son klasör/dosya adını ayıklar
// Örnek: "/home/anil" -> "anil", "/etc/configs" -> "configs", "/" -> "/"
const char* get_basename(const char* path) {
    if (!path || strcmp(path, "/") == 0) return "/";
    
    const char* last_slash = strrchr(path, '/');
    if (!last_slash) return path; // Eğik çizgi yoksa doğrudan kendisidir
    
    // Eğer yol '/' ile bitiyorsa (örn: /home/anil/) bir önceki karakteri aramak gerekebilir,
    // ancak vfs_resolve_path genellikle bunu temizler.
    return (last_slash[1] != '\0') ? &last_slash[1] : path;
}

void cmd_tree(int argc, char** argv) {
    char resolved[VFS_PATH_MAX];
    const char* input;

    // 1. Argüman yoksa mevcut çalışma dizinini (CWD) al, varsa girilen yolu al
    if (argc > 1) {
        input = argv[1];
    } else {
        // KuvixOS VFS yapısında muhtemelen güncel dizini dönen bir fonksiyon vardır.
        // Örnek olarak vfs_get_cwd kullanılmıştır, çekirdeğindeki tam karşılığıyla değiştirebilirsin.
        input = vfs_get_cwd(); 
        if (!input) {
            input = "/"; // Fallback: Her ihtimale karşı kök dizine düş
        }
    }
    
    // 2. Girilen veya CWD'den alınan yolu standartlaştır
    if (!vfs_resolve_path(input, resolved, sizeof(resolved))) {
        commands_puts("Hata: yol cozumlenemedi.\n");
        return;
    }

    // 3. Ağacın en tepesine sadece bulunulan klasörün adını yazdırıyoruz
    // /home/anil dizisindeysek ekrana ilk satırda sadece "anil" yazacak
    const char* base_name = get_basename(resolved);
    commands_puts((char*)base_name);
    commands_puts("\n");

    // 4. Çözümlenen dinamik yolu kvxfs_tree fonksiyonuna gönder
    // Not: kvxfs_tree fonksiyonunun recursive (özyinelemeli) iç fonksiyonuna 
    // derinlik (depth) olarak 0 veya 1 parametresi geçiyorsan, bu fonksiyonu da güncellemen gerekebilir.
    if (!kvxfs_tree(resolved)) {
        commands_puts("Hata: Dizin agaci listelenemedi veya boyle bir dizin yok.\n");
    }
}

REGISTER_COMMAND(tree, cmd_tree, "Dizin agacini hiyerarsik olarak gosterir");