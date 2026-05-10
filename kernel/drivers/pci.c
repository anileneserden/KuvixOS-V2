#include <kernel/drivers/pci.h>
#include <kernel/drivers/net/e1000.h>
#include <arch/x86/io.h>
#include <kernel/printk.h>

// Envanter için maksimum cihaz sayısı
#define MAX_PCI_DEVICES 64

// Cihazları saklayacağımız liste (DDK/Loader buradan okuyacak)
static pci_device_t pci_devices[MAX_PCI_DEVICES];
static int pci_device_count = 0;

/**
 * PCI Konfigürasyon Adresi Hesaplama
 */
static inline uint32_t pci_cfg_addr(uint8_t bus, uint8_t slot, uint8_t func, uint8_t off) {
    return (1u << 31)
        | ((uint32_t)bus  << 16)
        | ((uint32_t)slot << 11)
        | ((uint32_t)func << 8)
        | (off & 0xFC);
}

/**
 * Temel Okuma/Yazma Fonksiyonları
 */
uint32_t pci_read32(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset) {
    outl(0xCF8, pci_cfg_addr(bus, slot, func, offset));
    return inl(0xCFC);
}

void pci_write32(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset, uint32_t value) {
    outl(0xCF8, pci_cfg_addr(bus, slot, func, offset));
    outl(0xCFC, value);
}

void pci_write16(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset, uint16_t value) {
    uint32_t old = pci_read32(bus, slot, func, offset & 0xFC);
    uint32_t shift = (offset & 2) * 8;
    uint32_t mask = 0xFFFFu << shift;
    uint32_t neu = (old & ~mask) | ((uint32_t)value << shift);
    pci_write32(bus, slot, func, offset & 0xFC, neu);
}

uint16_t pci_read16(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset) {
    uint32_t v = pci_read32(bus, slot, func, offset & 0xFC);
    return (v >> ((offset & 2) * 8)) & 0xFFFF;
}

uint8_t pci_read8(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset) {
    uint32_t v = pci_read32(bus, slot, func, offset & 0xFC);
    return (v >> ((offset & 3) * 8)) & 0xFF;
}

/**
 * PCI Veriyolunu Tarar ve Cihazları Envantere Kaydeder
 */
void pci_init(void) {
    pci_device_count = 0;
    printk("[PCI] Veriyolu taraniyor ve cihazlar kaydediliyor...\n");

    for (uint16_t bus = 0; bus < 256; bus++) {
        for (uint8_t slot = 0; slot < 32; slot++) {
            for (uint8_t func = 0; func < 8; func++) {
                uint16_t vendor = pci_read16(bus, slot, func, 0x00);
                
                // Cihaz yoksa atla
                if (vendor == 0xFFFF) {
                    if (func == 0) break; 
                    continue;
                }

                // Envanterde yer varsa kaydet
                if (pci_device_count < MAX_PCI_DEVICES) {
                    pci_device_t* dev = &pci_devices[pci_device_count++];
                    
                    dev->bus = (uint8_t)bus;
                    dev->slot = slot;
                    dev->func = func;
                    dev->vendor = vendor;
                    dev->device = pci_read16(bus, slot, func, 0x02);
                    dev->class_id = pci_read8(bus, slot, func, 0x0B);
                    dev->subclass = pci_read8(bus, slot, func, 0x0A);
                    
                    // BAR0 (Base Address Register) - Donanım erişimi için ilk kapı
                    dev->bar0 = pci_read32(bus, slot, func, 0x10);

                    printk("[PCI] Cihaz bulundu! Vendor: %d, Device: %d, Bus: %d, Slot: %d\n", 
                        (uint32_t)dev->vendor, (uint32_t)dev->device, 
                        (uint32_t)dev->bus, (uint32_t)dev->slot);

                    // --- OTOMATİK SÜRÜCÜ BAĞLAMA (Opsiyonel) ---
                    // Eğer e1000 kernel içindeyse hala buradan çağırabilirsin
                    if (dev->class_id == 0x02 && dev->vendor == 0x8086 && dev->device == 0x100E) {
                        printk("[PCI] Intel e1000 tespit edildi, probe baslatiliyor...\n");
                        e1000_probe(bus, slot, func);
                    }
                }

                // Çok fonksiyonlu cihaz kontrolü (Header Type bit 7)
                if (func == 0) {
                    uint8_t hdr = pci_read8(bus, slot, func, 0x0E);
                    if ((hdr & 0x80) == 0) break;
                }
            }
        }
    }
    printk("[PCI] Tarama tamamlandi. Toplam %d cihaz envantere alindi.\n", pci_device_count);
}

/**
 * DDK/Loader için yardımcı erişim fonksiyonları
 */
int pci_get_device_count(void) {
    return pci_device_count;
}

pci_device_t* pci_get_device(int index) {
    if (index >= 0 && index < pci_device_count) {
        return &pci_devices[index];
    }
    return (void*)0;
}