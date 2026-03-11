#include <kernel/drivers/usb/xhci.h>
#include <kernel/drivers/pci.h>
#include <arch/x86/io.h>
#include <kernel/printk.h>

#define PCI_CONFIG_ADDRESS 0xCF8
#define PCI_CONFIG_DATA    0xCFC

// ---- PCI Temel Erişim Fonksiyonları ----

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

void pci_write32(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset, uint32_t val) {
    outl(PCI_CONFIG_ADDRESS, pci_addr(bus, slot, func, offset));
    outl(PCI_CONFIG_DATA, val);
}

void pci_enable_mmio(uint8_t bus, uint8_t slot, uint8_t func) {
    // Command Register (0x04) üzerinden Memory Space ve Bus Master yetkisi ver
    uint32_t cmd = pci_read32(bus, slot, func, 0x04);
    cmd |= (1 << 1) | (1 << 2); 
    pci_write32(bus, slot, func, 0x04, cmd);
}

// ---- PCI Tarama ve Detaylandırma ----

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
            if (pci_read16((uint8_t)bus, slot, 0, 0x00) == 0xFFFF) continue;
            
            pci_read_dev((uint8_t)bus, slot, 0, &d);
            cb(&d, user);

            if (d.header_type & 0x80) { // Multi-function cihazları da tara
                for (uint8_t func = 1; func < 8; func++) {
                    if (pci_read16((uint8_t)bus, slot, func, 0x00) == 0xFFFF) continue;
                    pci_read_dev((uint8_t)bus, slot, func, &d);
                    cb(&d, user);
                }
            }
        }
    }
}

// ---- USB ve xHCI Spesifik Bölüm ----

static int g_any = 0;
static int g_usb = 0;

static void any_cb(const pci_dev_t* dev, void* user) {
    (void)user;
    g_any++;

    // Cihaz bulma logu
    if (dev->class_code == 0x0C && dev->subclass == 0x03 && dev->prog_if == 0x30) {
        g_usb++;
        printk("[PCI] Found xHCI Controller: %x:%x.%u\n", dev->bus, dev->slot, dev->func);

        uint32_t bar0 = pci_read32(dev->bus, dev->slot, dev->func, 0x10);
        uint32_t mmio_addr = bar0 & 0xFFFFFFF0;

        // 1. Donanıma erişim izni ver
        pci_enable_mmio(dev->bus, dev->slot, dev->func);

        // 2. Sürücüyü sadece BİR kere çağır
        printk("[PCI] Starting xHCI Driver...\n");
        
        // SADECE bunu çağırıyoruz. 
        // xhci_debug_dump çağrısını sildik çünkü o da içeride init'i çağırıyor.
        xhci_minimal_init(mmio_addr); 
    }
}

// kmain.c içinden çağrılan ana fonksiyon
void pci_debug_list_usb(void) {
    g_any = 0;
    g_usb = 0;
    printk("[PCI] Scanning all devices...\n");
    pci_enumerate(any_cb, 0);
    printk("[PCI] Scan finished. xHCI devices found: %d\n", g_usb);
}