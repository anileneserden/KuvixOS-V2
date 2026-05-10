#ifndef LOADER_H
#define LOADER_H

#include <stdint.h>
#include <kernel/drivers/pci.h>

/**
 * Genel Sürücü Yapısı
 */
typedef struct {
    char name[32];
    int (*init)(void);
    void (*exit)(void);
} driver_t;

/**
 * Kernel API Tablosu
 * Modüllere (kmod) sunulan tüm "hizmetler" burada toplanır.
 */
typedef struct {
    // Temel Fonksiyonlar
    void (*printk)(const char* fmt, ...);
    void (*register_driver)(driver_t* drv);
    
    // --- PCI Envanter API ---
    int (*get_pci_count)(void);
    pci_device_t* (*get_pci_device)(int index);

    // --- Doğrudan PCI Donanım Erişimi ---
    uint16_t (*pci_read16)(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset);
    void (*pci_write16)(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset, uint16_t val);
    uint32_t (*pci_read32)(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset);
    void (*pci_write32)(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset, uint32_t val);
} kernel_api_t;

int load_module_from_file(const char* path);

#endif