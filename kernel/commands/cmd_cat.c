#include <kernel/fs.h>
#include <kernel/printk.h>
#include <kernel/kmalloc.h>
#include <lib/string.h>
#include <lib/commands.h>

void cmd_touch(int argc, char** argv) {
    if (argc < 2) {
        printk("Kullanim: touch <dosya_adi>\n");
        return;
    }

    // 1. Yeni düğüm için yer ayır
    fs_node_t *new_file = (fs_node_t*)kmalloc(sizeof(fs_node_t));
    memset(new_file, 0, sizeof(fs_node_t));

    // 2. Dosya bilgilerini doldur
    strcpy(new_file->name, argv[1]);
    new_file->flags = FS_FILE;
    new_file->length = 0;
    new_file->read = 0; // Şimdilik boş dosya
    
    // 3. Bağlı listenin sonuna ekle (fs_root -> next zinciri)
    if (fs_root->next == 0) {
        fs_root->next = new_file;
    } else {
        fs_node_t *curr = fs_root->next;
        while (curr->next != 0) {
            curr = curr->next;
        }
        curr->next = new_file;
    }

    printk("Dosya '%s' olusturuldu.\n", argv[1]);
}

REGISTER_COMMAND(touch, cmd_touch, "Yeni bir bos dosya olusturur");