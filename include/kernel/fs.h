#pragma once
#include <stdint.h>

#define FS_FILE        0x01
#define FS_DIRECTORY   0x02

struct fs_node;

// Fonksiyon işaretçileri (vtable benzeri yapı)
typedef uint32_t (*read_type_t)(struct fs_node*, uint32_t, uint32_t, uint8_t*);
typedef struct fs_node* (*finddir_type_t)(struct fs_node*, char *name);

typedef struct fs_node {
    char name[128];
    uint32_t flags;
    uint32_t length;
    uintptr_t data;          // Dosya içeriğinin RAM adresi
    read_type_t read;       
    finddir_type_t finddir; 
    struct fs_node *next;    // Bağlı liste yapısı
} fs_node_t;

// --- Global Değişkenler ---
extern fs_node_t *fs_root;    // Sistemin kök dizini (/)
extern fs_node_t *fs_current; // Shell'in bulunduğu aktif dizin

// --- VFS Fonksiyon Prototipleri ---

/**
 * Dosya sisteminden veri okur.
 */
uint32_t read_fs(fs_node_t *node, uint32_t offset, uint32_t size, uint8_t *buffer);

/**
 * Bir dizin içerisinde isimle arama yapar.
 */
fs_node_t *finddir_fs(fs_node_t *node, char *name);

/**
 * VFS listesinden bir düğümü (dosya/dizin) kaldırır.
 */
void vfs_remove_node(fs_node_t *parent, char *name);

/**
 * RamFS sürücüsünü başlatır ve root node döndürür.
 * (kmain.c tarafından çağrılır)
 */
fs_node_t *init_ramfs(void);