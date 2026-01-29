#include <kernel/fs.h>
#include <lib/string.h>

fs_node_t root_nodes[3]; // 2 dosya + 1 boş bitiş elemanı
fs_node_t ram_root;

// Dosyayı okumaya çalıştığımızda bu mesajı döneceğiz
uint32_t ramfs_read_test(fs_node_t *node, uint32_t offset, uint32_t size, uint8_t *buffer) {
    char *msg = "KuvixOS RamFS Test Mesaji!";
    uint32_t len = strlen(msg);
    if (offset >= len) return 0;
    if (offset + size > len) size = len - offset;
    memcpy(buffer, msg + offset, size);
    return size;
}

fs_node_t *init_ramfs() {
    // 1. Kök Dizin Ayarları
    strcpy(ram_root.name, "root");
    ram_root.flags = FS_DIRECTORY;
    ram_root.ptr = &root_nodes[0]; // İçeriği root_nodes dizisine bağla

    // 2. İlk Dosyamız
    strcpy(root_nodes[0].name, "test.txt");
    root_nodes[0].flags = FS_FILE;
    root_nodes[0].length = 26;
    root_nodes[0].read = &ramfs_read_test;

    // 3. İkinci Dosyamız
    strcpy(root_nodes[1].name, "kuvix.txt");
    root_nodes[1].flags = FS_FILE;
    root_nodes[1].length = 26;
    root_nodes[1].read = &ramfs_read_test;

    // Bitiş işareti (Döngüler için)
    root_nodes[2].name[0] = '\0';

    return &ram_root;
}