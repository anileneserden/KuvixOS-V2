#include <kernel/drivers/usb/xhci.h>
#include <kernel/printk.h>
#include <kernel/memory/kmalloc.h>
#include <lib/string.h> // memset

static inline uint8_t  mmio_read8 (uint32_t base, uint32_t off) { return *(volatile uint8_t *)(base + off); }
static inline uint16_t mmio_read16(uint32_t base, uint32_t off) { return *(volatile uint16_t*)(base + off); }
static inline uint32_t mmio_read32(uint32_t base, uint32_t off) { return *(volatile uint32_t*)(base + off); }
static inline void     mmio_write32(uint32_t base, uint32_t off, uint32_t v) { *(volatile uint32_t*)(base + off) = v; }

static void* xhci_alloc_aligned(uint32_t size, uint32_t align, uint32_t* out_phys);

static void delay(volatile uint32_t n) { while (n--) { __asm__ __volatile__("nop"); } }

// ---- TRB layout (16 bytes) ----
typedef struct __attribute__((packed)) {
    uint32_t d0;
    uint32_t d1;
    uint32_t d2;
    uint32_t d3;
} xhci_trb_t;

// TRB types
enum {
    TRB_TYPE_NORMAL              = 1,
    TRB_TYPE_SETUP_STAGE         = 2,
    TRB_TYPE_DATA_STAGE          = 3,
    TRB_TYPE_STATUS_STAGE        = 4,
    TRB_TYPE_LINK                = 6,
    TRB_TYPE_ENABLE_SLOT_CMD     = 9,
    TRB_TYPE_CMD_COMPLETION_EVT  = 33,
};

static inline uint32_t trb_type(uint32_t d3) { return (d3 >> 10) & 0x3F; }

// Command Ring state
static xhci_trb_t* g_cmd_ring = 0;
static uint32_t g_cmd_ring_phys = 0;
static uint32_t g_cmd_ring_cycle = 1;
static uint32_t g_cmd_ring_index = 0;
static uint32_t g_cmd_ring_size = 256; // TRB count

// Event Ring state (very minimal, 1 segment)
static xhci_trb_t* g_evt_ring = 0;
static uint32_t g_evt_ring_phys = 0;
static uint32_t g_evt_ring_cycle = 1;
static uint32_t g_evt_ring_size = 256;

static uint32_t g_xhci_mmio = 0;
static uint8_t  g_xhci_caplen = 0;
static uint32_t g_xhci_max_ports = 0;
static uint8_t  g_xhci_ports = 0;
static uint32_t g_last_ccs_mask = 0;
static int      g_xhci_ready = 0;

// ERST entry (16 bytes)
typedef struct __attribute__((packed)) {
    uint32_t seg_base_lo;
    uint32_t seg_base_hi;
    uint32_t seg_size;
    uint32_t rsvd;
} xhci_erst_entry_t;

static xhci_erst_entry_t* g_erst = 0;
static uint32_t g_erst_phys = 0;

// DCBAA (array of pointers)
static uint64_t* g_dcbaa = 0;
static uint32_t g_dcbaa_phys = 0;

void xhci_set_global(uint32_t mmio) {
    g_xhci_mmio = mmio;
    g_xhci_caplen = mmio_read8(mmio, 0x00);

    uint32_t hcs1 = mmio_read32(mmio, 0x04);
    g_xhci_max_ports = (hcs1 >> 24) & 0xFF;

    if (g_xhci_max_ports > 32) g_xhci_max_ports = 32; // safety
}

uint32_t xhci_get_portsc(uint32_t port_1based) {
    if (!g_xhci_mmio) return 0;
    if (port_1based < 1 || port_1based > g_xhci_max_ports) return 0;

    uint32_t op = g_xhci_mmio + g_xhci_caplen;
    return mmio_read32(op, 0x400 + (port_1based - 1) * 0x10);
}

uint32_t xhci_get_max_ports(void) { return g_xhci_max_ports; }

static void xhci_controller_reset(uint32_t op) {
    uint32_t usbcmd = mmio_read32(op, 0x00);
    uint32_t usbsts = mmio_read32(op, 0x04);
    printk("[xHCI] reset: USBCMD=%x USBSTS=%x\n", usbcmd, usbsts);

    usbcmd &= ~1u;                 // Stop
    mmio_write32(op, 0x00, usbcmd);

    delay(5000000);

    mmio_write32(op, 0x00, usbcmd | (1u << 1)); // HCRST
    for (volatile uint32_t i = 0; i < 20000000; i++) {
        uint32_t v = mmio_read32(op, 0x00);
        if ((v & (1u << 1)) == 0) break;
    }

    usbcmd = mmio_read32(op, 0x00);
    usbsts = mmio_read32(op, 0x04);
    printk("[xHCI] after HCRST: USBCMD=%x USBSTS=%x\n", usbcmd, usbsts);
}

static void xhci_init_rings(uint32_t mmio, uint32_t caplen, uint32_t dboff, uint32_t rtsoff) {
    (void)dboff;

    // ---- Allocate Command Ring ----
    g_cmd_ring = (xhci_trb_t*)xhci_alloc_aligned(sizeof(xhci_trb_t) * g_cmd_ring_size, 64, &g_cmd_ring_phys);
    memset(g_cmd_ring, 0, sizeof(xhci_trb_t) * g_cmd_ring_size);
    g_cmd_ring_phys = (uint32_t)(uintptr_t)g_cmd_ring;
    g_cmd_ring_cycle = 1;
    g_cmd_ring_index = 0;

    // Put a LINK TRB at the end to make the ring circular
    uint32_t last = g_cmd_ring_size - 1;
    uint32_t link_addr = g_cmd_ring_phys; // point back to start

    g_cmd_ring[last].d0 = link_addr;
    g_cmd_ring[last].d1 = 0;
    g_cmd_ring[last].d2 = 0;
    // TYPE=LINK (6), TC=1 (toggle cycle)
    g_cmd_ring[last].d3 = (TRB_TYPE_LINK << 10) | (1u << 1); // TC=1, C bit set later by hw? We'll set C=cycle.
    // Set C bit to current producer cycle
    g_cmd_ring[last].d3 |= (g_cmd_ring_cycle & 1u);

    // ---- Allocate Event Ring + ERST ----
    g_evt_ring = (xhci_trb_t*)xhci_alloc_aligned(sizeof(xhci_trb_t) * g_evt_ring_size, 64, &g_evt_ring_phys);
    memset(g_evt_ring, 0, sizeof(xhci_trb_t) * g_evt_ring_size);
    g_evt_ring_phys = (uint32_t)(uintptr_t)g_evt_ring;
    g_evt_ring_cycle = 1;

    g_erst = (xhci_erst_entry_t*)xhci_alloc_aligned(sizeof(xhci_erst_entry_t) * 1, 64, &g_erst_phys);
    memset(g_erst, 0, sizeof(xhci_erst_entry_t));
    g_erst_phys = (uint32_t)(uintptr_t)g_erst;

    g_erst[0].seg_base_lo = g_evt_ring_phys;
    g_erst[0].seg_base_hi = 0;
    g_erst[0].seg_size    = g_evt_ring_size;
    g_erst[0].rsvd        = 0;

    // ---- DCBAA ----
    // spec: size = max_slots+1, but we can allocate 257 pointers safely
    g_dcbaa = (uint64_t*)xhci_alloc_aligned(sizeof(uint64_t) * 257, 64, &g_dcbaa_phys);
    memset(g_dcbaa, 0, sizeof(uint64_t) * 257);
    g_dcbaa_phys = (uint32_t)(uintptr_t)g_dcbaa;

    // ---- Program runtime interrupter 0 ----
    // Runtime regs base: mmio + RTSOFF
    uint32_t rt = mmio + (rtsoff & ~0x1F);
    uint32_t intr0 = rt + 0x20; // Interrupter Register Set 0 base (per spec)

    // ERSTSZ
    mmio_write32(intr0, 0x08, 1); // 1 entry
    // ERSTBA (64-bit)
    mmio_write32(intr0, 0x10, g_erst_phys);
    mmio_write32(intr0, 0x14, 0);
    // ERDP (dequeue ptr) -> start of event ring
    mmio_write32(intr0, 0x18, g_evt_ring_phys);
    mmio_write32(intr0, 0x1C, 0);

    // Enable interrupter (IMAN bit0=1)
    uint32_t iman = mmio_read32(intr0, 0x00);
    mmio_write32(intr0, 0x00, iman | 1u);

    printk("[xHCI] rings: cmd=%x evt=%x erst=%x dcbaa=%x\n",
           g_cmd_ring_phys, g_evt_ring_phys, g_erst_phys, g_dcbaa_phys);
}

static void* xhci_alloc_aligned(uint32_t size, uint32_t align, uint32_t* out_phys) {
    // align power-of-two varsayımı
    uint32_t raw = (uint32_t)(uintptr_t)kmalloc(size + align);
    uint32_t aligned = (raw + (align - 1)) & ~(align - 1);

    if (out_phys) *out_phys = aligned;

    // not: free yoksa raw'ı saklamaya gerek yok. (ileride free istersen bir table tutarız)
    memset((void*)aligned, 0, size);
    return (void*)aligned;
}

static void xhci_program_operational(uint32_t mmio, uint32_t caplen) {
    uint32_t op = mmio + caplen;

    // MaxSlotsEn -> CONFIG (OP + 0x38)
    uint32_t hcs1 = mmio_read32(mmio, 0x04);         // HCSPARAMS1 CAP reg offset 0x04
    uint32_t max_slots = hcs1 & 0xFF;
    if (max_slots == 0) max_slots = 1;

    mmio_write32(op, 0x38, max_slots);              // CONFIG
    printk("[xHCI] CONFIG max_slots_en=%u\n", (unsigned)max_slots);

    // Set DCBAAP (Operational + 0x30, 64-bit)
    mmio_write32(op, 0x30, g_dcbaa_phys);
    mmio_write32(op, 0x34, 0);

    // Set CRCR (Operational + 0x18, 64-bit)
    // Bits: [0]=RCS (ring cycle state), [3:1]=reserved, [5:4]=Command Ring Running/Stopped? (RO)
    uint32_t crcr_lo = (g_cmd_ring_phys & ~0x3Fu) | (g_cmd_ring_cycle & 1u);
    mmio_write32(op, 0x18, crcr_lo);
    mmio_write32(op, 0x1C, 0);

    printk("[xHCI] DCBAAP=%x CRCR=%x\n", g_dcbaa_phys, crcr_lo);
}

static void xhci_run(uint32_t mmio, uint32_t caplen) {
    uint32_t op = mmio + caplen;
    uint32_t usbcmd = mmio_read32(op, 0x00);
    usbcmd |= 1u; // Run/Stop = 1
    mmio_write32(op, 0x00, usbcmd);
    delay(2000000);
    uint32_t usbsts = mmio_read32(op, 0x04);
    printk("[xHCI] run: USBCMD=%x USBSTS=%x\n", usbcmd, usbsts);
}

static void xhci_ring_doorbell0(uint32_t mmio, uint32_t dboff) {
    // Doorbell array base: mmio + DBOFF (aligned)
    uint32_t db = mmio + (dboff & ~0x3);
    // Doorbell 0, write target=0 (command ring)
    mmio_write32(db, 0x00, 0);
}

static void xhci_cmd_push(xhci_trb_t trb) {
    // place TRB at current index
    uint32_t idx = g_cmd_ring_index;

    // If we're at last-1, next is LINK TRB; keep room
    if (idx >= g_cmd_ring_size - 1) idx = 0;

    trb.d3 |= (g_cmd_ring_cycle & 1u); // set C bit
    g_cmd_ring[idx] = trb;

    g_cmd_ring_index = idx + 1;
    if (g_cmd_ring_index >= g_cmd_ring_size - 1) {
        // next is LINK; producer moves to 0 and toggles cycle because LINK has TC=1
        g_cmd_ring_index = 0;
        g_cmd_ring_cycle ^= 1u;
    }
}

static int xhci_poll_cmd_completion(uint32_t mmio, uint32_t rtsoff) {
    for (uint32_t spin = 0; spin < 40000000; spin++) {
        for (uint32_t i = 0; i < g_evt_ring_size; i++) {
            xhci_trb_t* e = &g_evt_ring[i];
            uint32_t d3 = e->d3;

            uint32_t c = d3 & 1u;
            if (c != (g_evt_ring_cycle & 1u)) continue;

            uint32_t type = (d3 >> 10) & 0x3F;
            if (type == TRB_TYPE_CMD_COMPLETION_EVT) {
                uint32_t cc = (e->d2 >> 24) & 0xFF;
                uint32_t slot_id = (d3 >> 24) & 0xFF;

                printk("[xHCI] CMD COMPLETION: idx=%u cc=%u slot=%u\n",
                       (unsigned)i, (unsigned)cc, (unsigned)slot_id);

                // şimdilik: sadece cycle toggle (ultra-minimal)
                g_evt_ring_cycle ^= 1u;
                return (int)slot_id;
            } else {
                printk("[xHCI] EVT type=%u idx=%u d3=%x\n", (unsigned)type, (unsigned)i, d3);
                g_evt_ring_cycle ^= 1u;
                return -1;
            }
        }
    }

    printk("[xHCI] poll timeout waiting command completion\n");
    return -1;
}

static void xhci_try_reset_port1(uint32_t op) {
    uint32_t off = 0x400; // PORT1
    uint32_t portsc = mmio_read32(op, off);

    uint32_t ccs = portsc & 1u;
    printk("[xHCI] PORT1 pre-reset PORTSC=%x ccs=%u\n", portsc, (unsigned)ccs);
    if (!ccs) {
        printk("[xHCI] PORT1: no device connected, skip reset\n");
        return;
    }

    // Clear change bits (W1C): CSC/PEC/WRC
    mmio_write32(op, off, (1u<<17) | (1u<<18) | (1u<<19));

    // Start Port Reset (PR)
    mmio_write32(op, off, (1u<<4));

    // crude delay
    for (volatile uint32_t i = 0; i < 2000000; i++) { }

    uint32_t after = mmio_read32(op, off);
    uint32_t ped = (after >> 1) & 1u;
    uint32_t pr  = (after >> 4) & 1u;
    uint32_t pls = (after >> 5) & 0xFu;

    printk("[xHCI] PORT1 post-reset PORTSC=%x ped=%u pr=%u pls=%u\n",
           after, (unsigned)ped, (unsigned)pr, (unsigned)pls);
}

void xhci_debug_dump(uint32_t mmio) {
    // xHCI Capability Registers
    uint8_t  caplen = mmio_read8(mmio, 0x00);
    uint16_t hciver = mmio_read16(mmio, 0x02);
    uint32_t hcs1   = mmio_read32(mmio, 0x04);
    uint32_t hcs2   = mmio_read32(mmio, 0x08);
    uint32_t hcs3   = mmio_read32(mmio, 0x0C);
    uint32_t hcc1   = mmio_read32(mmio, 0x10);
    uint32_t dboff  = mmio_read32(mmio, 0x14);
    uint32_t rtsoff = mmio_read32(mmio, 0x18);

    printk("[xHCI] MMIO=%x CAPLENGTH=%x HCIVERSION=%x\n",
           mmio, (unsigned)caplen, (unsigned)hciver);
    printk("[xHCI] HCSPARAMS1=%x HCSPARAMS2=%x HCSPARAMS3=%x\n",
           hcs1, hcs2, hcs3);
    printk("[xHCI] HCCPARAMS1=%x DBOFF=%x RTSOFF=%x\n",
           hcc1, dboff, rtsoff);

    // HCSPARAMS1 decode
    uint32_t max_ports = (hcs1 >> 24) & 0xFF;
    uint32_t max_intrs = (hcs1 >> 8)  & 0x7FF;
    uint32_t max_slots = (hcs1 >> 0)  & 0xFF;

    printk("[xHCI] max_slots=%u max_intrs=%u max_ports=%u\n",
           (unsigned)max_slots, (unsigned)max_intrs, (unsigned)max_ports);

    // Operational Registers base = mmio + CAPLENGTH
    uint32_t op = mmio + caplen;
    uint32_t usbcmd = mmio_read32(op, 0x00);
    uint32_t usbsts = mmio_read32(op, 0x04);
    printk("[xHCI] OPBASE=%x USBCMD=%x USBSTS=%x\n", op, usbcmd, usbsts);

    // PortSC: op + 0x400 + 0x10*(port-1)
    // Şimdilik ilk 8 portu yaz
    uint32_t limit = max_ports;
    if (limit > 8) limit = 8;

    for (uint32_t p = 1; p <= limit; p++) {
        uint32_t portsc = mmio_read32(op, 0x400 + (p - 1) * 0x10);
        printk("[xHCI] PORT%u PORTSC=%x\n", (unsigned)p, portsc);
    }

    xhci_controller_reset(op);
    xhci_try_reset_port1(op);
}

void xhci_minimal_init(uint32_t mmio) {
    // Capability regs
    uint8_t  caplen = mmio_read8(mmio, 0x00);
    uint32_t dboff  = mmio_read32(mmio, 0x14);
    uint32_t rtsoff = mmio_read32(mmio, 0x18);

    uint32_t op = mmio + caplen;

    printk("[xHCI] minimal_init begin (mmio=%x caplen=%x)\n", mmio, (unsigned)caplen);

    xhci_controller_reset(op);

    xhci_init_rings(mmio, caplen, dboff, rtsoff);
    xhci_program_operational(mmio, caplen);
    xhci_run(mmio, caplen);

    // ---- Send Enable Slot Command ----
    xhci_trb_t trb;
    memset(&trb, 0, sizeof(trb));
    trb.d0 = 0;
    trb.d1 = 0;
    trb.d2 = 0;
    trb.d3 = (TRB_TYPE_ENABLE_SLOT_CMD << 10);

    xhci_cmd_push(trb);
    xhci_ring_doorbell0(mmio, dboff);

    int slot = xhci_poll_cmd_completion(mmio, rtsoff);
    printk("[xHCI] Enable Slot result slot=%d\n", slot);
}

int xhci_poll_hotplug(void) {
    if (!g_xhci_ready || !g_xhci_mmio || !g_xhci_caplen) return 0;

    uint32_t op = g_xhci_mmio + g_xhci_caplen;

    uint32_t mask = 0;
    uint32_t limit = g_xhci_ports;
    if (limit == 0) limit = 1;
    if (limit > 32) limit = 32;

    for (uint32_t p = 1; p <= limit; p++) {
        uint32_t portsc = mmio_read32(op, 0x400 + (p - 1) * 0x10);
        uint32_t ccs = portsc & 1u;
        if (ccs) mask |= (1u << (p - 1));
    }

    uint32_t changed = (mask ^ g_last_ccs_mask);
    if (!changed) return 0;

    // tek event döndürelim (ilk değişen port)
    for (uint32_t p = 1; p <= limit; p++) {
        uint32_t bit = (1u << (p - 1));
        if (!(changed & bit)) continue;

        int now_connected = (mask & bit) ? 1 : 0;

        g_last_ccs_mask = mask;

        // istersen change bitlerini W1C temizleyebilirsin (şimdilik şart değil)
        // mmio_write32(op, 0x400 + (p - 1) * 0x10, (1u<<17)); // CSC

        return now_connected ? (int)p : -(int)p;
    }

    g_last_ccs_mask = mask;
    return 0;
}