#include <kernel/fs.h>
#include <lib/string.h>
#include <kernel/kmalloc.h>

fs_node_t *ramfs_finddir(fs_node_t *node, char *name) {
    fs_node_t *curr = node->next;
    while (curr != 0) {
        if (strcmp(name, curr->name) == 0)
            return curr;
        curr = curr->next;
    }
    return 0;
}

fs_node_t *init_ramfs(void) {
    fs_node_t *ram_root = (fs_node_t*)kmalloc(sizeof(fs_node_t));
    memset(ram_root, 0, sizeof(fs_node_t));
    
    strcpy(ram_root->name, "root");
    ram_root->flags = FS_DIRECTORY;
    ram_root->finddir = &ramfs_finddir;
    
    return ram_root;
}

uint32_t ramfs_read(fs_node_t *node, uint32_t offset, uint32_t size, uint8_t *buffer) {
    if (!node->data || offset > node->length) return 0;

    // Okunacak miktarı dosya boyutuyla sınırla
    if (offset + size > node->length) {
        size = node->length - offset;
    }

    // Bellekten hedef buffer'a veriyi aktar
    memcpy(buffer, (uint8_t*)(node->data + offset), size);
    return size;
}