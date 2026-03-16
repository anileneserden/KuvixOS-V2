#include <lib/commands.h>
#include <kernel/printk.h>
#include <lib/shell.h> // shell_get_cwd fonksiyonuna erişmek için

void cmd_pwd(int argc, char** argv) {
    // pwd komutu genellikle parametre almaz, alsa da ignore edebiliriz.
    // shell.c içindeki o anki dizini çekiyoruz.
    const char* current_dir = shell_get_cwd();
    
    if (current_dir) {
        printk("%s\n", current_dir);
    } else {
        printk("Hata: Mevcut dizin bilgisi alınamadı.\n");
    }
}

// Makron ile komutu sisteme kaydediyoruz
REGISTER_COMMAND(pwd, cmd_pwd, "Mevcut calisma dizinini yazdirir");