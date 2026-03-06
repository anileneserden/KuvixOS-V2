// kernel/drivers/net/e1000.c
#include <kernel/drivers/net/e1000.h>
#include <kernel/drivers/net/pci.h>
#include <kernel/printk.h>
#include <lib/string.h>
#include <stdint.h>

// ------------------------------------------------------------
// Registers
// ------------------------------------------------------------
#define E1000_REG_CTRL      0x0000
#define E1000_REG_STATUS    0x0008

#define E1000_REG_ICR       0x00C0
#define E1000_REG_IMS       0x00D0

#define E1000_REG_TCTL      0x0400
#define E1000_REG_TIPG      0x0410

#define E1000_REG_TDBAL     0x3800
#define E1000_REG_TDBAH     0x3804
#define E1000_REG_TDLEN     0x3808
#define E1000_REG_TDH       0x3810
#define E1000_REG_TDT       0x3818

#define E1000_REG_RAL0      0x5400
#define E1000_REG_RAH0      0x5404

// ------------------------------------------------------------
// Bits
// ------------------------------------------------------------
#define E1000_CTRL_RST      (1u << 26)

#define E1000_TCTL_EN       (1u << 1)
#define E1000_TCTL_PSP      (1u << 3)

#define E1000_TXD_CMD_EOP   (1u << 0)
#define E1000_TXD_CMD_IFCS  (1u << 1)
#define E1000_TXD_CMD_RS    (1u << 3)
#define E1000_TXD_STAT_DD   (1u << 0)

// ------------------------------------------------------------
// MMIO helpers (identity map varsayımı)
// ------------------------------------------------------------
static inline uint32_t mmio_read32_raw(uint32_t base, uint32_t off) {
    volatile uint32_t* p = (volatile uint32_t*)(uintptr_t)(base + off);
    return *p;
}
static inline void mmio_write32_raw(uint32_t base, uint32_t off, uint32_t v) {
    volatile uint32_t* p = (volatile uint32_t*)(uintptr_t)(base + off);
    *p = v;
}

// Basit delay (kaba)
static void io_delay(void) {
    for (volatile int i = 0; i < 200000; i++) { }
}

// ------------------------------------------------------------
// TX ring
// ------------------------------------------------------------
typedef struct __attribute__((packed)) {
    uint64_t addr;
    uint16_t length;
    uint8_t  cso;
    uint8_t  cmd;
    uint8_t  status;
    uint8_t  css;
    uint16_t special;
} e1000_tx_desc_t;

#define TX_RING_SIZE  8
#define TX_BUF_SIZE   2048

static volatile e1000_tx_desc_t* g_tx_ring = 0;
static uint8_t*                  g_tx_bufs = 0;
static uint32_t                  g_tx_next = 0;

// DMA test için: statik + aligned
static volatile e1000_tx_desc_t g_tx_ring_static[TX_RING_SIZE] __attribute__((aligned(16)));
static uint8_t g_tx_bufs_static[TX_RING_SIZE][TX_BUF_SIZE] __attribute__((aligned(16)));

// ------------------------------------------------------------
// MAC
// ------------------------------------------------------------
static void e1000_dump_mac(uint32_t mmio) {
    uint32_t ral = mmio_read32_raw(mmio, E1000_REG_RAL0);
    uint32_t rah = mmio_read32_raw(mmio, E1000_REG_RAH0);

    uint8_t mac[6];
    mac[0] = (uint8_t)(ral & 0xFF);
    mac[1] = (uint8_t)((ral >> 8) & 0xFF);
    mac[2] = (uint8_t)((ral >> 16) & 0xFF);
    mac[3] = (uint8_t)((ral >> 24) & 0xFF);
    mac[4] = (uint8_t)(rah & 0xFF);
    mac[5] = (uint8_t)((rah >> 8) & 0xFF);

    printk("[E1000] MAC %x:%x:%x:%x:%x:%x (RAL=%x RAH=%x)\n",
           mac[0], mac[1], mac[2], mac[3], mac[4], mac[5], ral, rah);
}

// ------------------------------------------------------------
// TX init + send
// ------------------------------------------------------------
static void e1000_tx_init(uint32_t mmio) {
    // statik ring/buf kullan (DMA teşhisi için)
    g_tx_ring = g_tx_ring_static;
    g_tx_bufs = (uint8_t*)g_tx_bufs_static;

    memset(g_tx_ring_static, 0, sizeof(g_tx_ring_static));
    memset(g_tx_bufs_static, 0, sizeof(g_tx_bufs_static));

    // debug hizalama
    printk("[E1000] TX ring static=%x bufs static=%x\n",
           (uint32_t)(uintptr_t)g_tx_ring_static,
           (uint32_t)(uintptr_t)g_tx_bufs_static);

    for (uint32_t i = 0; i < TX_RING_SIZE; i++) {
        g_tx_ring_static[i].addr   = (uint32_t)(uintptr_t)&g_tx_bufs_static[i][0];
        g_tx_ring_static[i].status = E1000_TXD_STAT_DD;
    }

    uint32_t ring_phys = (uint32_t)(uintptr_t)g_tx_ring_static;

    mmio_write32_raw(mmio, E1000_REG_TDBAL, ring_phys);
    mmio_write32_raw(mmio, E1000_REG_TDBAH, 0);
    mmio_write32_raw(mmio, E1000_REG_TDLEN, TX_RING_SIZE * sizeof(e1000_tx_desc_t));
    mmio_write32_raw(mmio, E1000_REG_TDH, 0);
    mmio_write32_raw(mmio, E1000_REG_TDT, 0);

    mmio_write32_raw(mmio, E1000_REG_TCTL,
                     E1000_TCTL_EN | E1000_TCTL_PSP | (0x10u << 4) | (0x40u << 12));
    mmio_write32_raw(mmio, E1000_REG_TIPG, 0x0060200A);

    g_tx_next = 0;

    printk("[E1000] TX init ring=%x bufs=%x\n",
           ring_phys, (uint32_t)(uintptr_t)g_tx_bufs_static);
}

static void e1000_send_test_frame(uint32_t mmio) {
    if (!g_tx_ring) {
        printk("[E1000] TX send skipped: not initialized\n");
        return;
    }

    uint32_t idx = g_tx_next % TX_RING_SIZE;
    volatile e1000_tx_desc_t* d = &g_tx_ring_static[idx];

    if ((d->status & E1000_TXD_STAT_DD) == 0) {
        printk("[E1000] TX desc busy idx=%u status=%x\n", idx, d->status);
        return;
    }

    uint8_t* pkt = &g_tx_bufs_static[idx][0];
    uint32_t p = 0;

    // dst MAC = broadcast
    for (int i = 0; i < 6; i++) pkt[p++] = 0xFF;

    // src MAC = RAL/RAH
    uint32_t ral = mmio_read32_raw(mmio, E1000_REG_RAL0);
    uint32_t rah = mmio_read32_raw(mmio, E1000_REG_RAH0);
    pkt[p++] = (uint8_t)(ral & 0xFF);
    pkt[p++] = (uint8_t)((ral >> 8) & 0xFF);
    pkt[p++] = (uint8_t)((ral >> 16) & 0xFF);
    pkt[p++] = (uint8_t)((ral >> 24) & 0xFF);
    pkt[p++] = (uint8_t)(rah & 0xFF);
    pkt[p++] = (uint8_t)((rah >> 8) & 0xFF);

    // EtherType = 0x88B5 (test)
    pkt[p++] = 0x88;
    pkt[p++] = 0xB5;

    const char* msg = "KuvixOS e1000 TX test frame";
    for (int i = 0; msg[i] && p < 60; i++) {
        pkt[p++] = (uint8_t)msg[i];
    }

    while (p < 60) pkt[p++] = 0;

    d->length = (uint16_t)p;
    d->cso    = 0;
    d->cmd    = E1000_TXD_CMD_EOP | E1000_TXD_CMD_IFCS | E1000_TXD_CMD_RS;
    d->status = 0;

    uint32_t tdt = (idx + 1) % TX_RING_SIZE;
    mmio_write32_raw(mmio, E1000_REG_TDT, tdt);

    // completion bekle
    for (int spin = 0; spin < 3000000; spin++) {
        uint8_t st = d->status;              // volatile read
        if (st & E1000_TXD_STAT_DD) break;
    }
    
    // ek debug: TX register durumları
    uint32_t tdh_r = mmio_read32_raw(mmio, E1000_REG_TDH);
    uint32_t tdt_r = mmio_read32_raw(mmio, E1000_REG_TDT);
    uint32_t tctl  = mmio_read32_raw(mmio, E1000_REG_TCTL);

    printk("[E1000] TX sent idx=%u len=%u status=%x kickTDT=%u\n",
           idx, (unsigned)d->length, (unsigned)d->status, (unsigned)tdt);
    printk("[E1000] TX regs: TDH=%u TDT=%u TCTL=%x\n",
           (unsigned)tdh_r, (unsigned)tdt_r, (unsigned)tctl);

    g_tx_next++;
}

// ------------------------------------------------------------
// Probe entry
// ------------------------------------------------------------
int e1000_probe(uint8_t bus, uint8_t slot, uint8_t func) {
    uint16_t vid = pci_read16(bus, slot, func, 0x00);
    uint16_t did = pci_read16(bus, slot, func, 0x02);

    if (vid != 0x8086) return 0;
    if (did != 0x100E) return 0;

    // PCI COMMAND: Memory + BusMaster aç
    uint16_t cmd = pci_read16(bus, slot, func, 0x04);
    cmd |= (1u << 1); // Memory Space Enable
    cmd |= (1u << 2); // Bus Master Enable
    pci_write16(bus, slot, func, 0x04, cmd);

    uint32_t bar0_raw = pci_read32(bus, slot, func, 0x10);
    uint32_t mmio     = bar0_raw & ~0xFu;

    printk("[E1000] found at %u:%u.%u vid=%x did=%x\n", bus, slot, func, vid, did);
    printk("[E1000] PCI CMD=%x BAR0 raw=%x base=%x\n", (unsigned)cmd, bar0_raw, mmio);

    uint32_t ctrl   = mmio_read32_raw(mmio, E1000_REG_CTRL);
    uint32_t status = mmio_read32_raw(mmio, E1000_REG_STATUS);
    printk("[E1000] CTRL=%x STATUS=%x\n", ctrl, status);

    e1000_dump_mac(mmio);

    (void)mmio_read32_raw(mmio, E1000_REG_ICR);
    mmio_write32_raw(mmio, E1000_REG_IMS, 0);

    mmio_write32_raw(mmio, E1000_REG_CTRL, ctrl | E1000_CTRL_RST);
    io_delay();

    printk("[E1000] after reset CTRL=%x STATUS=%x\n",
           mmio_read32_raw(mmio, E1000_REG_CTRL),
           mmio_read32_raw(mmio, E1000_REG_STATUS));

    e1000_tx_init(mmio);
    e1000_send_test_frame(mmio);

    return 1;
}