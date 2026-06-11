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
    uint32_t magic;           
    uint32_t driver_version;  
    char     driver_name[32]; 
    uint32_t init_offset;     
    uint32_t exit_offset;     
    uint32_t code_size;       
} __attribute__((packed)) KDF_Header;

int kdf_load_driver(const char* path);

#endif