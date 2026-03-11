#include <kernel/drivers/usb/xhci.h>
#include <kernel/printk.h>
#include <kernel/memory/kmalloc.h>
#include <lib/string.h>

// ---- MMIO Yardımcı Fonksiyonları ----
static inline uint8_t  mmio_read8 (uint32_t base, uint32_t off) { return *(volatile uint8_t *)(base + off); }
static inline uint32_t mmio_read32(uint32_t base, uint32_t off) { return *(volatile uint32_t*)(base + off); }
static inline void     mmio_write32(uint32_t base, uint32_t off, uint32_t v) { *(volatile uint32_t*)(base + off) = v; }

// 64-bit Güvenli Yazma: Üst 32-bit (offset + 4) kısmını mutlaka sıfırlıyoruz.
static inline void mmio_write64(uint32_t base, uint32_t off, uint32_t low) {
    mmio_write32(base, off, low);
    mmio_write32(base, off + 4, 0);
}

static void delay(volatile uint32_t n) { while (n--) { __asm__ __volatile__("nop"); } }

// ---- xHCI Veri Yapıları ----
typedef struct __attribute__((packed)) {
    uint32_t d0, d1, d2, d3;
} xhci_trb_t;

typedef struct __attribute__((packed)) {
    uint32_t d[8];      // Slot Context
    uint32_t ep0[8];    // Endpoint 0 Context
    uint32_t rsvd[30 * 8]; 
} xhci_device_context_t;

typedef struct __attribute__((packed)) {
    uint32_t drop_flags;
    uint32_t add_flags;
    uint32_t rsvd[6];
    xhci_device_context_t dev;
} xhci_input_context_t;

enum {
    TRB_TYPE_LINK = 6, 
    TRB_TYPE_ENABLE_SLOT_CMD = 9,
    TRB_TYPE_ADDRESS_DEVICE = 11, 
    TRB_TYPE_CMD_COMPLETION_EVT = 33,
};

// ---- Global Durum Değişkenleri ----
static xhci_trb_t* g_cmd_ring = 0;
static uint32_t g_cmd_ring_phys = 0;
static uint32_t g_cmd_ring_cycle = 1;
static uint32_t g_cmd_ring_index = 0;
static uint32_t g_cmd_ring_size = 256;

static xhci_trb_t* g_evt_ring = 0;
static uint32_t g_evt_ring_phys = 0;
static uint32_t g_evt_ring_cycle = 1;
static uint32_t g_evt_ring_index = 0;
static uint32_t g_evt_ring_size = 256;

static uint32_t g_dcbaa_phys = 0;
static uint32_t g_xhci_mmio_base = 0;
static uint8_t  g_xhci_caplen = 0;
static uint32_t g_xhci_max_ports = 0;

// ---- Desktop.c ve Linker İçin Gerekli Fonksiyonlar ----

void xhci_set_global(uint32_t mmio) {
    g_xhci_mmio_base = mmio;
    g_xhci_caplen = mmio_read8(mmio, 0x00);
    // HCS1 (Structural Parameters 1) register'ından max port sayısını al
    uint32_t hcs1 = mmio_read32(mmio, 0x04);
    g_xhci_max_ports = (hcs1 >> 24) & 0xFF;
}

uint32_t xhci_get_max_ports(void) { return g_xhci_max_ports; }

uint32_t xhci_get_portsc(uint32_t port_1based) {
    if (!g_xhci_mmio_base) return 0;
    uint32_t op = g_xhci_mmio_base + g_xhci_caplen;
    // Port status registerları 0x400 offsetinden başlar, her biri 16 byte'tır.
    return mmio_read32(op, 0x400 + (port_1based - 1) * 0x10);
}

int xhci_poll_hotplug(void) { return 0; }

void xhci_debug_dump(uint32_t mmio) {
    xhci_minimal_init(mmio); 
}

// ---- Çekirdek (Core) Fonksiyonlar ----

static void* xhci_alloc_aligned(uint32_t size, uint32_t align, uint32_t* out_phys) {
    // kmalloc 16-byte hizaladığı için 64-byte boundary garantisi için padding ekliyoruz
    uint32_t raw = (uint32_t)(uintptr_t)kmalloc(size + align + 64);
    if (!raw) return 0;

    uint32_t aligned = (raw + (align - 1)) & ~(align - 1);
    if (out_phys) *out_phys = aligned;

    memset((void*)aligned, 0, size);
    return (void*)aligned;
}

static void xhci_cmd_push(xhci_trb_t trb) {
    // Cycle bitini TRB'ye işle
    trb.d3 = (trb.d3 & ~1u) | (g_cmd_ring_cycle & 1u);
    g_cmd_ring[g_cmd_ring_index++] = trb;

    // Halka sonu kontrolü
    if (g_cmd_ring_index >= g_cmd_ring_size - 1) {
        xhci_trb_t link = { g_cmd_ring_phys, 0, 0, (TRB_TYPE_LINK << 10) | (1u << 1) | (g_cmd_ring_cycle & 1u) };
        g_cmd_ring[g_cmd_ring_index] = link;
        g_cmd_ring_index = 0;
        g_cmd_ring_cycle ^= 1u;
    }
}

static int xhci_poll_cmd_completion(uint32_t mmio, uint32_t rtsoff) {
    uint32_t intr0 = mmio + (rtsoff & ~0x1F) + 0x20;

    for (uint32_t spin = 0; spin < 5000000; spin++) {
        xhci_trb_t* e = &g_evt_ring[g_evt_ring_index];
        
        // Donanım TRB'yi yazdı mı? (Cycle bit kontrolü)
        if ((e->d3 & 1u) == (g_evt_ring_cycle & 1u)) {
            uint32_t type = (e->d3 >> 10) & 0x3F;
            uint32_t res = (e->d3 >> 24) & 0xFF; // Slot ID buradadır
            
            g_evt_ring_index++;
            if (g_evt_ring_index >= g_evt_ring_size) {
                g_evt_ring_index = 0;
                g_evt_ring_cycle ^= 1u;
            }

            // ERDP (Event Ring Dequeue Pointer) güncelle ve EHB bitini temizle
            uint32_t erdp_val = g_evt_ring_phys + (g_evt_ring_index * sizeof(xhci_trb_t));
            mmio_write64(intr0, 0x18, erdp_val | 0x8);

            if (type == TRB_TYPE_CMD_COMPLETION_EVT) return (int)res;
        }
        delay(100);
    }
    return -1;
}

void xhci_minimal_init(uint32_t mmio) {
    printk("[xHCI] Starting Minimal Init at %x...\n", mmio);
    xhci_set_global(mmio);
    
    uint32_t dboff = mmio_read32(mmio, 0x14);
    uint32_t rtsoff = mmio_read32(mmio, 0x18);
    uint32_t op = mmio + g_xhci_caplen;

    // 1. Controller Reset
    mmio_write32(op, 0, mmio_read32(op, 0) & ~1u); // Stop
    delay(100000);
    mmio_write32(op, 0, 2); // Reset
    while(mmio_read32(op, 0) & 2);
    printk("[xHCI] Reset done.\n");

    // 2. Bellek Alanlarını Ayır (64-byte Aligned)
    g_cmd_ring = xhci_alloc_aligned(sizeof(xhci_trb_t)*256, 64, &g_cmd_ring_phys);
    g_evt_ring = xhci_alloc_aligned(sizeof(xhci_trb_t)*256, 64, &g_evt_ring_phys);
    xhci_alloc_aligned(256 * 8, 64, &g_dcbaa_phys);
    
    uint32_t ep_phys = 0;
    uint32_t* erst = xhci_alloc_aligned(16, 64, &ep_phys);
    erst[0] = g_evt_ring_phys;
    erst[1] = 0;
    erst[2] = 256;
    erst[3] = 0;
    printk("[xHCI] Rings and DCBAA allocated.\n");

    // 3. Registerları Programla (64-bit Adres Temizliğiyle)
    mmio_write32(op, 0x38, 32);             // Max Slots
    mmio_write64(op, 0x30, g_dcbaa_phys);   // DCBAAP
    mmio_write64(op, 0x18, g_cmd_ring_phys | 1); // CRCR

    uint32_t intr0 = mmio + (rtsoff & ~0x1F) + 0x20;
    mmio_write32(intr0, 0x08, 1);           // ERST Size
    mmio_write64(intr0, 0x10, ep_phys);     // ERST Base
    mmio_write64(intr0, 0x18, g_evt_ring_phys); // ERDP

    // 4. Kontrolcüyü Çalıştır
    mmio_write32(op, 0, mmio_read32(op, 0) | 1);
    while(!(mmio_read32(op, 0x04) & 1)) { // Status register 'Halted' bitini kontrol et
        if (mmio_read32(op, 0) & 1) break;
    }
    printk("[xHCI] Controller is RUNNING.\n");

    // 5. Enable Slot Komutu
    g_evt_ring_cycle = 1;
    g_evt_ring_index = 0;

    printk("[xHCI] Sending Enable Slot command...\n");
    xhci_trb_t t = {0, 0, 0, (9u << 10)}; // TRB_TYPE_ENABLE_SLOT_CMD = 9
    xhci_cmd_push(t);
    
    // Doorbell 0'ı çal
    mmio_write32(mmio + dboff, 0, 0); 
    
    int slot = xhci_poll_cmd_completion(mmio, rtsoff);

    if (slot > 0) {
        printk("[xHCI] SUCCESS! Slot assigned: %d. Addressing device...\n", slot);
        
        // 1. Input Context Hazırla (Cihaz özelliklerini bildirmek için)
        uint32_t phys_in = 0;
        xhci_input_context_t* input = xhci_alloc_aligned(sizeof(xhci_input_context_t), 64, &phys_in);
        
        // Slot ve Endpoint 0'ı konfigüre edeceğimizi belirtiyoruz
        input->add_flags = 0x03; 
        
        // Cihazın hangi portta olduğunu söyle (QEMU'da genelde Port 1)
        uint32_t port_id = 1; 
        input->dev.d[0] = (1u << 27); // Route String (opsiyonel)
        input->dev.d[1] = (port_id << 16); // Root Hub Port Number
        
        // Endpoint 0 (Kontrol kanalı) özellikleri: Paket boyutu 64 byte (USB 2.0/3.0 için güvenli)
        input->dev.ep0[1] = (3 << 1) | (8 << 16); // EP State = Running, Max Packet Size = 8 (2^3*8=64)

        // 2. Device Context (Donanımın kendi yazacağı alan)
        uint32_t phys_ctx = 0;
        xhci_alloc_aligned(sizeof(xhci_device_context_t), 64, &phys_ctx);
        
        // DCBAA dizinine bu fiziksel adresi kaydet
        // g_dcbaa bir uint64_t dizisi olduğu için slot numarasını indeks olarak kullanıyoruz
        ((uint64_t*)g_dcbaa_phys)[slot] = (uint64_t)phys_ctx;

        // 3. Address Device Komutunu Gönder
        // d0 = Input Context Fiziksel Adresi
        // d3 = Slot ID (üst 8 bit) + Command Type (11)
        xhci_trb_t trb = { phys_in, 0, 0, (slot << 24) | (11u << 10) };
        xhci_cmd_push(trb);
        
        // Zili çal
        mmio_write32(mmio + dboff, 0, 0); 

        // Cevabı bekle
        if (xhci_poll_cmd_completion(mmio, rtsoff) >= 0) {
            printk("[xHCI] ADDRESS SUCCESS! Device is now in ADDRESSED state.\n");
            // Artık bu noktadan sonra USB descriptor'larını okuyabiliriz!
        } else {
            printk("[xHCI] Address Device failed/timeout.\n");
        }
    } else {
        printk("[xHCI] FAILED: Could not enable slot (result: %d)\n", slot);
    }
}