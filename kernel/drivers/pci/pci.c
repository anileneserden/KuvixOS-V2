#include <kernel/drivers/pci/pci.h>
#include <kernel/drivers/net/e1000.h>
#include <arch/x86/io.h>
#include <kernel/printk.h>
#include <kernel/drivers/usb/xhci.h>

static inline uint32_t pci_cfg_addr(uint8_t bus, uint8_t slot, uint8_t func, uint8_t off) {
    return (1u << 31)
        | ((uint32_t)bus  << 16)
        | ((uint32_t)slot << 11)
        | ((uint32_t)func << 8)
        | (off & 0xFC);
}

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

void check_usb_pci(void) {
    printk("[PCI] USB denetleyicileri taranıyor...\n");
    for (uint16_t bus = 0; bus < 256; bus++) {
        for (uint8_t slot = 0; slot < 32; slot++) {
            if (pci_read16(bus, slot, 0, 0x00) == 0xFFFF) continue;
            
            uint8_t class    = pci_read8(bus, slot, 0, 0x0B);
            uint8_t subclass = pci_read8(bus, slot, 0, 0x0A);

            // 0x0C: Serial Bus Controller, 0x03: USB Controller
            if (class == 0x0C && subclass == 0x03) {
                uint32_t bar0 = pci_read32(bus, slot, 0, 0x10);
                uint32_t base = bar0 & 0xFFFFFFF0; // Alt 4 biti maskele (BAR flagleri)
                
                // Command Register işlemleri
                uint16_t cmd = pci_read16(bus, slot, 0, 0x04);
                uint16_t new_cmd = cmd | 0x06; // 0x02 (Memory Space) | 0x04 (Bus Master)
                pci_write16(bus, slot, 0, 0x04, new_cmd);
                
                // Değişikliği doğrula
                uint16_t final_cmd = pci_read16(bus, slot, 0, 0x04);
                
                printk("-> USB Controller Bulundu: Bus %u, Slot %u\n", bus, slot);
                printk("   BAR0: 0x%x, CMD: 0x%x -> 0x%x\n", base, cmd, final_cmd);
                
                if (base != 0) {
                    xhci_init(base);
                } else {
                    printk("   [HATA] BAR0 adresi geçersiz!\n");
                }
            }
        }
    }
}

void pci_scan_dump_nics(void) {
    for (uint16_t bus = 0; bus < 256; bus++) {
        for (uint8_t slot = 0; slot < 32; slot++) {
            for (uint8_t func = 0; func < 8; func++) {
                uint16_t vendor = pci_read16(bus, slot, func, 0x00);
                if (vendor == 0xFFFF) {
                    if (func == 0) break;
                    continue;
                }

                uint16_t device = pci_read16(bus, slot, func, 0x02);
                uint8_t class   = pci_read8 (bus, slot, func, 0x0B);
                uint8_t subclass= pci_read8 (bus, slot, func, 0x0A);

                if (class == 0x02) {
                    printk("[PCI] NIC bus=%u slot=%u func=%u vendor=%x device=%x subclass=%x\n",
                        bus, slot, func, vendor, device, subclass);

                    if (vendor == 0x8086 && device == 0x100E) {
                        e1000_probe(bus, slot, func);
                    }
                }

                if (func == 0) {
                    uint8_t hdr = pci_read8(bus, slot, func, 0x0E);
                    if ((hdr & 0x80) == 0) break;
                }
            }
        }
    }
}