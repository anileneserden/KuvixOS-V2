#pragma once
#include <stdint.h>

#define FS_FILE        0x01
#define FS_DIRECTORY   0x02

struct fs_node;

typedef uint32_t (*read_type_t)(struct fs_node*, uint32_t, uint32_t, uint8_t*);
typedef struct fs_node* (*finddir_type_t)(struct fs_node*, char *name);

typedef struct fs_node {
    char name[128];
    uint32_t flags;
    uint32_t length;
    uintptr_t data;         // EKLE: Dosya içeriğinin RAM adresi
    read_type_t read;       
    finddir_type_t finddir; 
    struct fs_node *next;    
} fs_node_t;

extern fs_node_t *fs_root;
extern fs_node_t *fs_current;

uint32_t read_fs(fs_node_t *node, uint32_t offset, uint32_t size, uint8_t *buffer);
fs_node_t *finddir_fs(fs_node_t *node, char *name);