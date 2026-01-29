#ifndef FS_H
#define FS_H

#include <stdint.h>

#define FS_FILE        0x01
#define FS_DIRECTORY   0x02

struct fs_node;

// Dosya sistemine özel okuma/yazma fonksiyonlarının tipi
typedef uint32_t (*read_type_t)(struct fs_node*, uint32_t, uint32_t, uint8_t*);

typedef struct fs_node {
    char name[128];     // Dosyanın adı
    uint32_t flags;     // Dosya mı (0x01) yoksa Dizin mi (0x02)?
    uint32_t length;    // Byte cinsinden boyutu
    read_type_t read;   // Okuma fonksiyonu (Her dosya sistemi kendine göre yazar)
    struct fs_node *ptr; // Alt dosyalar (Dizin ise kullanılır)
} fs_node_t;

// Sistemin kök dizini (/)
extern fs_node_t *fs_root;

// VFS üzerinden okuma arayüzü
uint32_t read_fs(fs_node_t *node, uint32_t offset, uint32_t size, uint8_t *buffer);

#endif