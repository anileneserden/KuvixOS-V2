#include <kernel/fs.h>
#include <kernel/printk.h>
#include <lib/string.h>
#include <lib/commands.h> // REGISTER_COMMAND burada tanımlı olmalı

void cmd_ls(int argc, char** argv) {
    // VFS'in yüklü olduğunu kontrol et
    if (fs_root == 0) {
        printk("Hata: Dosya sistemi (VFS) yuklenemedi!\n");
        return;
    }

    // RamFS yapımızda kök dizin içeriği .ptr içinde duruyor
    fs_node_t *files = (fs_node_t *)fs_root->ptr;

    printk("Dizin listeleniyor: /\n");
    printk("Isim            Boyut       Tip\n");
    printk("----            -----       ---\n");

    // Dizi sonuna kadar (ismi boş olana kadar) dön
    for (int i = 0; files[i].name[0] != '\0'; i++) {
        printk("%s", files[i].name);
        
        // Sütun hizalaması için boşluk bırak
        int spaces = 16 - strlen(files[i].name);
        for(int s = 0; s < spaces; s++) printk(" ");

        printk("%d bytes    ", files[i].length);

        if (files[i].flags == FS_DIRECTORY) {
            printk("<DIR>\n");
        } else {
            printk("<FILE>\n");
        }
    }
}

// Komutu otomatik olarak sisteme kaydet
REGISTER_COMMAND(ls, cmd_ls, "Dizin içeriğini listeler");