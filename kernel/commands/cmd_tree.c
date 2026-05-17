#include <kernel/fs/vfs.h>
#include <lib/commands.h>
#include <lib/string.h>
#include <kernel/fs/kvxfs.h>
#include <kernel/printk.h>

void cmd_tree(int argc, char** argv) {
    char resolved[VFS_PATH_MAX];
    
    // 1. Kullanıcının girdiği yolu al, girmediyse varsayılan olarak kök dizini '/' hedefle
    const char* input = (argc > 1) ? argv[1] : "/";
    
    // 2. Girilen yolu VFS standartlarına göre çözümle (CWD veya bağıl yolları temizle)
    if (!vfs_resolve_path(input, resolved, sizeof(resolved))) {
        commands_puts("Hata: yol cozumlenemedi.\n");
        return;
    }

    // 3. STATİK KONTROLLER ÇÖPE: 
    // Doğrudan çözümlenen yolu kvxfs_tree fonksiyonuna gönderiyoruz.
    // Fonksiyon artık gelen yola göre (/etc, /home, /) diskteki ilgili alt ağacı tarayacak.
    if (!kvxfs_tree(resolved)) {
        commands_puts("Hata: Dizin agaci listelenemedi veya boyle bir dizin yok.\n");
    }
}

REGISTER_COMMAND(tree, cmd_tree, "Dizin agacini hiyerarsik olarak gosterir");