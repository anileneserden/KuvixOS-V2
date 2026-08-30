#ifndef KMOD_API_H
#define KMOD_API_H

#include <stdint.h>
#include <stddef.h>

// Çekirdeğin modüle sağlayacağı temel servisler
typedef struct {
    void (*printk)(const char* fmt, ...);
    void* (*kmalloc)(uint32_t size);
    void (*kfree)(void* ptr);
    int (*read_file)(const char* path, char* buffer, uint32_t max_size);
} KModKernelAPI;

// Dinamik modül operasyon ve sorgulama yapısı
typedef struct {
    // Modülün dışarıya isme göre operasyon (fonksiyon işaretçisi) döndürmesini sağlar
    void* (*get_operation)(const char* op_name);
} KModOperations;

#endif