#ifndef LOADER_H
#define LOADER_H

#include <stdint.h>

// --- ÇEKİRDEK VE SÜRÜCÜ API YAPILARI ---
typedef struct {
    void (*printk)(const char* fmt, ...);
    uint8_t (*inb)(uint16_t port);
    void (*outb)(uint16_t port, uint8_t data);
    void (*register_interrupt)(int irq, void (*handler)(void));
    uint32_t (*pci_read32)(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset);
    void (*pci_write32)(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset, uint32_t value);
    void* (*dma_alloc)(uint32_t size, uint32_t align);
} KernelAPI;

typedef struct {
    int (*read)(void* buffer, uint32_t size);
    int (*write)(const void* buffer, uint32_t size);
    int (*control)(const char* command, void* arg, uint32_t arg_size);
} KDF_Operations;

// RTC ve özel sürücüler için genişletilebilir operasyon yapıları
typedef struct {
    int (*read)(void* buffer, uint32_t size);
    int (*write)(const void* buffer, uint32_t size);
    int (*control)(const char* command, void* arg, uint32_t arg_size);
    int (*read_datetime)(void* out);
} RTC_KDF_Operations;

// --- FONKSİYON PROTOTİPLERİ ---
void load_desktop_module(const char* filepath);
void load_login_module(const char* filepath);
void load_driver_module(const char* filepath);
void load_command_module(const char* filepath, int argc, char** argv);
void load_user_module(const char* filepath);
void* getDriverFunction(const char* driver_name, const char* function_name);

#endif