#include <kernel/drivers/pci/pci.h>
#include <arch/x86/io.h>
#include <kernel/printk.h>

// Yeni eklediğimiz başlık dosyaları
#include <kernel/drivers/audio/hda.h>
#include <kernel/drivers/usb/host/xhci.h>

// e1000 hâlâ eski konumunda
extern void e1000_probe(uint8_t bus, uint8_t slot, uint8_t func);

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

void pci_scan_bus(void) {
    printk("[PCI] Scanning central PCI bus for hardware devices...\n");

    for (uint16_t bus = 0; bus < 256; bus++) {
        for (uint8_t slot = 0; slot < 32; slot++) {
            for (uint8_t func = 0; func < 8; func++) {
                uint16_t vendor = pci_read16(bus, slot, func, 0x00);
                if (vendor == 0xFFFF) {
                    if (func == 0) break;
                    continue;
                }

                uint16_t device = pci_read16(bus, slot, func, 0x02);
                uint8_t class   = pci_read8(bus, slot, func, 0x0B);
                uint8_t subclass = pci_read8(bus, slot, func, 0x0A);
                uint8_t prog_if  = pci_read8(bus, slot, func, 0x09);

                printk("[PCI] Device -> Bus:%u Slot:%u Func:%u | Vendor:%x Device:%x | Class:%x Sub:%x\n",
                       bus, slot, func, vendor, device, class, subclass);

                // 1. Ağ Kartları (Class 0x02)
                if (class == 0x02) {
                    if (vendor == 0x8086 && device == 0x100E) {
                        e1000_probe(bus, slot, func);
                    }
                }
                // 2. Ses Kartları (Class 0x04, Subclass 0x03)
                else if (class == 0x04 && subclass == 0x03) {
                    hda_audio_probe(bus, slot, func);
                }
                // 3. USB Kontrolcüleri (Class 0x0C, Subclass 0x03, xHCI için ProgIF 0x30)
                else if (class == 0x0C && subclass == 0x03 && prog_if == 0x30) {
                    xhci_usb_probe(bus, slot, func);
                }

                if (func == 0) {
                    uint8_t hdr = pci_read8(bus, slot, func, 0x0E);
                    if ((hdr & 0x80) == 0) break;
                }
            }
        }
    }
}
