#include <kernel/fs.h>

fs_node_t *fs_root = 0; // Başlangıçta kök boş

uint32_t read_fs(fs_node_t *node, uint32_t offset, uint32_t size, uint8_t *buffer) {
    // Eğer dosyanın kendine has bir okuma fonksiyonu varsa onu çağır
    if (node && node->read) {
        return node->read(node, offset, size, buffer);
    }
    return 0;
}