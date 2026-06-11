#ifndef KERNEL_KDF_H
#define KERNEL_KDF_H

#include <stdint.h>

#define KDF_MAGIC 0x46444B4B 

typedef struct {
    void (*printk)(const char* fmt, ...);
    uint8_t (*inb)(uint16_t port);
    void (*outb)(uint16_t port, uint8_t data);
    void (*register_interrupt)(int irq, void (*handler)(void));
} KernelAPI;

typedef struct {
    uint32_t magic;           // 4
    uint32_t driver_version;  // 4
    char     driver_name[32]; // 32
    uint32_t init_offset;     // 4
    uint32_t exit_offset;     // 4
    uint32_t code_size;       // 4
} __attribute__((packed)) KDF_Header;

int kdf_load_driver(const char* path);

#endif