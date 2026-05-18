#pragma once
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// PCI Konfigürasyon Alanından Okuma Fonksiyonları
uint32_t pci_read32(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset);
uint16_t pci_read16(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset);
uint8_t  pci_read8 (uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset);

// PCI Konfigürasyon Alanına Yazma Fonksiyonları
void pci_write32(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset, uint32_t value);
void pci_write16(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset, uint16_t value);

// Yeni Genel Amaçlı PCI Veri Yolu Tarayıcısı
void pci_scan_bus(void);

#ifdef __cplusplus
}
#endif