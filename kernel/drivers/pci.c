#include <kernel/drivers/usb/xhci.h>
#include <kernel/drivers/pci.h>
#include <arch/x86/io.h>
#include <kernel/printk.h>

#define PCI_CONFIG_ADDRESS 0xCF8
#define PCI_CONFIG_DATA    0xCFC

static inline uint8_t  mmio_read8 (uint32_t base, uint32_t off) { return *(volatile uint8_t *)(base + off); }
static inline uint16_t mmio_read16(uint32_t base, uint32_t off) { return *(volatile uint16_t*)(base + off); }
static inline uint32_t mmio_read32(uint32_t base, uint32_t off) { return *(volatile uint32_t*)(base + off); }

static inline uint32_t pci_addr(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset) {
    return (1u << 31) |
           ((uint32_t)bus  << 16) |
           ((uint32_t)slot << 11) |
           ((uint32_t)func <<  8) |
           (offset & 0xFC);
}

uint32_t pci_read32(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset) {
    outl(PCI_CONFIG_ADDRESS, pci_addr(bus, slot, func, offset));
    return inl(PCI_CONFIG_DATA);
}

uint16_t pci_read16(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset) {
    uint32_t v = pci_read32(bus, slot, func, offset);
    return (uint16_t)((v >> ((offset & 2) * 8)) & 0xFFFF);
}

uint8_t pci_read8(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset) {
    uint32_t v = pci_read32(bus, slot, func, offset);
    return (uint8_t)((v >> ((offset & 3) * 8)) & 0xFF);
}

static void pci_read_dev(uint8_t bus, uint8_t slot, uint8_t func, pci_dev_t* out) {
    out->bus = bus;
    out->slot = slot;
    out->func = func;

    out->vendor_id = pci_read16(bus, slot, func, 0x00);
    out->device_id = pci_read16(bus, slot, func, 0x02);

    out->prog_if    = pci_read8(bus, slot, func, 0x09);
    out->subclass   = pci_read8(bus, slot, func, 0x0A);
    out->class_code = pci_read8(bus, slot, func, 0x0B);

    out->header_type = pci_read8(bus, slot, func, 0x0E);
}

void pci_enumerate(void (*cb)(const pci_dev_t* dev, void* user), void* user) {
    pci_dev_t d;

    for (uint16_t bus = 0; bus < 256; bus++) {
        for (uint8_t slot = 0; slot < 32; slot++) {
            uint16_t vendor0 = pci_read16((uint8_t)bus, slot, 0, 0x00);
            if (vendor0 == 0xFFFF) continue;

            pci_read_dev((uint8_t)bus, slot, 0, &d);
            cb(&d, user);

            if (d.header_type & 0x80) {
                for (uint8_t func = 1; func < 8; func++) {
                    uint16_t vendor = pci_read16((uint8_t)bus, slot, func, 0x00);
                    if (vendor == 0xFFFF) continue;

                    pci_read_dev((uint8_t)bus, slot, func, &d);
                    cb(&d, user);
                }
            }
        }
    }
}

static const char* usb_prog_if_name(uint8_t prog_if) {
    switch (prog_if) {
        case 0x00: return "UHCI";
        case 0x10: return "OHCI";
        case 0x20: return "EHCI";
        case 0x30: return "xHCI";
        default:   return "USB(?)";
    }
}

static int g_any = 0;
static int g_usb = 0;

static void any_cb(const pci_dev_t* dev, void* user) {
    (void)user;
    g_any++;

    if (g_any <= 20) {
        printk("[PCI] dev %x:%x.%u vid=%x did=%x class=%x sub=%x if=%x\n",
               dev->bus, dev->slot, dev->func,
               dev->vendor_id, dev->device_id,
               dev->class_code, dev->subclass, dev->prog_if);
    }

    if (dev->class_code != 0x0C || dev->subclass != 0x03) return;

    g_usb++;

    printk("[PCI] USB controller %s at %x:%x.%u vid=%x did=%x prog_if=%x\n",
           usb_prog_if_name(dev->prog_if),
           dev->bus, dev->slot, dev->func,
           dev->vendor_id, dev->device_id,
           dev->prog_if);

    // BAR0..BAR5 RAW (0 olsa bile yazdır)
    for (int i = 0; i < 6; i++) {
        uint8_t off = (uint8_t)(0x10 + i * 4);
        uint32_t bar = pci_read32(dev->bus, dev->slot, dev->func, off);
        printk("    BAR%d raw=%x\n", i, bar);
    }

    // BAR0 decode (xHCI genelde BAR0 MMIO)
    uint32_t bar0 = pci_read32(dev->bus, dev->slot, dev->func, 0x10);
    uint32_t bar1 = pci_read32(dev->bus, dev->slot, dev->func, 0x14);

    if (bar0 == 0) {
        printk("    BAR0 is 0 (MMIO not assigned?)\n");
        return;
    }

    if (bar0 & 0x1) {
        // I/O space
        uint32_t io = (bar0 & 0xFFFFFFFC);
        printk("    BAR0 IO base=%x\n", io);
        return;
    }

    // Memory space
    uint32_t mem_type = (bar0 >> 1) & 0x3; // 0=32-bit, 2=64-bit
    uint32_t mmio_lo = (bar0 & 0xFFFFFFF0);

    if (mem_type == 2) {
        printk("    BAR0 is 64-bit MMIO: lo=%x hi=%x\n", mmio_lo, bar1);
        if (bar1 != 0) printk("    WARNING: MMIO above 4GB...\n");
    } else {
        printk("    BAR0 is 32-bit MMIO: base=%x\n", mmio_lo);
    }

    // Hemen sonra:
    printk("[PCI] calling xHCI dump...\n");
    xhci_debug_dump(mmio_lo);
    xhci_minimal_init(mmio_lo);
    xhci_set_global(mmio_lo);
}

void pci_debug_list_usb(void) {
    g_any = 0;
    g_usb = 0;

    printk("[PCI] scanning for USB controllers...\n");
    pci_enumerate(any_cb, 0);
    printk("[PCI] scan done: any=%d usb=%d\n", g_any, g_usb);
}
