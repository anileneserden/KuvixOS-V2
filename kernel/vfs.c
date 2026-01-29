#include <kernel/fs.h>

fs_node_t *fs_root = 0;

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