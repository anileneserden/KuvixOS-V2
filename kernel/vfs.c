#include <kernel/fs.h>
#include <kernel/printk.h>
#include <lib/string.h>

fs_node_t *fs_root = 0;
fs_node_t *fs_current = 0;

uint32_t read_fs(fs_node_t *node, uint32_t offset, uint32_t size, uint8_t *buffer) {
    if (node && node->read) {
        return node->read(node, offset, size, buffer);
    }
    return 0;
}

fs_node_t *finddir_fs(fs_node_t *node, char *name) {
    if (node && (node->flags & FS_DIRECTORY) && node->finddir) {
        return node->finddir(node, name);
    }
    return 0;
}

void vfs_remove_node(fs_node_t *parent, char *name) {
    fs_node_t *prev = parent;
    fs_node_t *curr = parent->next;

    while (curr) {
        if (strcmp(curr->name, name) == 0) {
            // Bağlantıyı kopar
            prev->next = curr->next;
            
            // Belleği serbest bırak (kmalloc kullandıysan kfree gerekir)
            // Eğer kfree henüz yoksa sadece listeden çıkarmış oluruz
            // kfree(curr); 
            
            printk("'%s' silindi.\n", name);
            return;
        }
        prev = curr;
        curr = curr->next;
    }
    printk("Hata: '%s' bulunamadi.\n", name);
}