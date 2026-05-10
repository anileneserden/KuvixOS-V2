#pragma once
#include <stdint.h>

// PCI Cihaz Yapısı
typedef struct {
    uint16_t vendor;
    uint16_t device;
    uint8_t bus;
    uint8_t slot;
    uint8_t func;
    uint8_t class_id;
    uint8_t subclass;
    uint32_t bar0;
} pci_device_t;

// Temel Okuma/Yazma
uint32_t pci_read32(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset);
uint16_t pci_read16(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset);
uint8_t  pci_read8 (uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset);
void pci_write32(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset, uint32_t value);
void pci_write16(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset, uint16_t value);

// Envanter Fonksiyonları
void pci_init(void); // Taramayı başlatır
int pci_get_device_count(void);
pci_device_t* pci_get_device(int index);