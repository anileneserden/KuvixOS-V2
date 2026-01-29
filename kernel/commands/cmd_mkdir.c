#include <kernel/fs.h>
#include <kernel/kmalloc.h>
#include <lib/string.h>
#include <lib/commands.h>
#include <kernel/printk.h>

void cmd_mkdir(int argc, char** argv) {
    if (argc < 2) {
        printk("Kullanim: mkdir <dizin_adi>\n");
        return;
    }

    fs_node_t *node = (fs_node_t*)kmalloc(sizeof(fs_node_t));
    memset(node, 0, sizeof(fs_node_t));

    strcpy(node->name, argv[1]);
    node->flags = FS_DIRECTORY; // ÖNEMLİ: Bu bir klasör
    node->length = 0;
    
    // Mevcut dizine (CWD) ekle
    node->next = fs_current->next;
    fs_current->next = node;

    printk("Dizin olusturuldu: %s\n", argv[1]);
}
REGISTER_COMMAND(mkdir, cmd_mkdir, "Yeni bir dizin olusturur");