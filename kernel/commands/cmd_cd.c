#include <kernel/fs.h>
#include <kernel/printk.h>
#include <lib/string.h>
#include <lib/commands.h>

void cmd_cd(int argc, char** argv) {
    if (argc < 2) {
        printk("Kullanim: cd <dizin_adi>\n");
        return;
    }

    // "cd .." ile bir üst dizine (şimdilik köke) dönme desteği
    if (strcmp(argv[1], "..") == 0) {
        fs_current = fs_root;
        printk("Kok dizine donuldu.\n");
        return;
    }

    // Mevcut dizinde hedef klasörü ara
    fs_node_t *target = finddir_fs(fs_current, argv[1]);

    if (target) {
        if (target->flags & FS_DIRECTORY) {
            fs_current = target; // Dizin bulundu, oraya gir!
            printk("Dizin degistirildi: %s\n", target->name);
        } else {
            printk("Hata: '%s' bir dizin degil!\n", argv[1]);
        }
    } else {
        printk("Hata: Dizin bulunamadi: %s\n", argv[1]);
    }
}

REGISTER_COMMAND(cd, cmd_cd, "Calisma dizinini degistirir");