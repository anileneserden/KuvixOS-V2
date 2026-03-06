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

#define E1000_REG_RCTL      0x0100

#define E1000_REG_TCTL      0x0400
#define E1000_REG_TIPG      0x0410

// TX ring registers
#define E1000_REG_TDBAL     0x3800
#define E1000_REG_TDBAH     0x3804
#define E1000_REG_TDLEN     0x3808
#define E1000_REG_TDH       0x3810
#define E1000_REG_TDT       0x3818

// RX ring registers
#define E1000_REG_RDBAL     0x2800
#define E1000_REG_RDBAH     0x2804
#define E1000_REG_RDLEN     0x2808
#define E1000_REG_RDH       0x2810
#define E1000_REG_RDT       0x2818

#define E1000_REG_RAL0      0x5400
#define E1000_REG_RAH0      0x5404

// ------------------------------------------------------------
// Bits
// ------------------------------------------------------------
#define E1000_CTRL_RST      (1u << 26)

#define E1000_TCTL_EN       (1u << 1)
#define E1000_TCTL_PSP      (1u << 3)

// RCTL bits (e1000)
#define E1000_RCTL_EN       (1u << 1)
#define E1000_RCTL_SBP      (1u << 2)
#define E1000_RCTL_UPE      (1u << 3)
#define E1000_RCTL_MPE      (1u << 4)
#define E1000_RCTL_LPE      (1u << 5)
#define E1000_RCTL_BAM      (1u << 15)

#define E1000_RCTL_BSIZE_2048 (0u << 16)

#define E1000_RCTL_LBM_NO   (0u << 6)
#define E1000_RCTL_LBM_MAC  (1u << 6)   // MAC loopback

// TX descriptor cmd/status
#define E1000_TXD_CMD_EOP   (1u << 0)
#define E1000_TXD_CMD_IFCS  (1u << 1)
#define E1000_TXD_CMD_RS    (1u << 3)
#define E1000_TXD_STAT_DD   (1u << 0)

// RX descriptor status
#define E1000_RXD_STAT_DD   (1u << 0)
#define E1000_RXD_STAT_EOP  (1u << 1)

#define E1000_CTRL_SLU   (1u << 6)  // Set Link Up
#define E1000_CTRL_ASDE  (1u << 5)  // Auto-Speed Detect Enable

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

#define E1000_STATUS_LU  (1u << 1)

static void e1000_force_link_up(uint32_t mmio) {
    uint32_t c = mmio_read32_raw(mmio, E1000_REG_CTRL);
    c |= E1000_CTRL_SLU | E1000_CTRL_ASDE;
    mmio_write32_raw(mmio, E1000_REG_CTRL, c);

    // link up bekle
    for (int i = 0; i < 2000000; i++) {
        uint32_t st = mmio_read32_raw(mmio, E1000_REG_STATUS);
        if (st & E1000_STATUS_LU) {
            printk("[E1000] link up STATUS=%x\n", st);
            return;
        }
    }
    printk("[E1000] WARN: link not up STATUS=%x\n", mmio_read32_raw(mmio, E1000_REG_STATUS));
}

// ------------------------------------------------------------
// Descriptors
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

typedef struct __attribute__((packed)) {
    uint64_t addr;
    uint16_t length;
    uint16_t checksum;
    uint8_t  status;
    uint8_t  errors;
    uint16_t special;
} e1000_rx_desc_t;

// ------------------------------------------------------------
// TX (statik + aligned)
// ------------------------------------------------------------
#define TX_RING_SIZE  8
#define TX_BUF_SIZE   2048

static volatile e1000_tx_desc_t g_tx_ring[TX_RING_SIZE] __attribute__((aligned(16)));
static uint8_t g_tx_bufs[TX_RING_SIZE][TX_BUF_SIZE] __attribute__((aligned(16)));
static uint32_t g_tx_next = 0;

// ------------------------------------------------------------
// RX (statik + aligned)
// ------------------------------------------------------------
#define RX_RING_SIZE  32
#define RX_BUF_SIZE   2048

static volatile e1000_rx_desc_t g_rx_ring[RX_RING_SIZE] __attribute__((aligned(16)));
static uint8_t g_rx_bufs[RX_RING_SIZE][RX_BUF_SIZE] __attribute__((aligned(16)));
static uint32_t g_rx_next = 0;

// ------------------------------------------------------------
// Utils
// ------------------------------------------------------------
static void dump_hex16(const uint8_t* b, uint32_t n) {
    // 16 byte satır
    uint32_t i = 0;
    while (i < n) {
        char line[128];
        uint32_t p = 0;

        // offset
        // basit yazım: "  xxxx: "
        uint32_t off = i;
        const char* hex = "0123456789ABCDEF";
        line[p++] = ' ';
        line[p++] = hex[(off >> 12) & 0xF];
        line[p++] = hex[(off >> 8)  & 0xF];
        line[p++] = hex[(off >> 4)  & 0xF];
        line[p++] = hex[(off >> 0)  & 0xF];
        line[p++] = ':';
        line[p++] = ' ';

        for (int k = 0; k < 16; k++) {
            if (i + (uint32_t)k < n) {
                uint8_t v = b[i + (uint32_t)k];
                line[p++] = hex[(v >> 4) & 0xF];
                line[p++] = hex[(v >> 0) & 0xF];
            } else {
                line[p++] = ' ';
                line[p++] = ' ';
            }
            line[p++] = ' ';
        }

        line[p] = 0;
        printk("[E1000][RX] %s\n", line);

        i += 16;
    }
}

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
    memset((void*)g_tx_ring, 0, sizeof(g_tx_ring));
    memset((void*)g_tx_bufs, 0, sizeof(g_tx_bufs));

    for (uint32_t i = 0; i < TX_RING_SIZE; i++) {
        g_tx_ring[i].addr   = (uint32_t)(uintptr_t)&g_tx_bufs[i][0];
        g_tx_ring[i].status = E1000_TXD_STAT_DD;
    }

    uint32_t ring_phys = (uint32_t)(uintptr_t)g_tx_ring;

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
           ring_phys, (uint32_t)(uintptr_t)g_tx_bufs);
}

static void e1000_send_test_frame(uint32_t mmio) {
    uint32_t idx = g_tx_next % TX_RING_SIZE;
    volatile e1000_tx_desc_t* d = &g_tx_ring[idx];

    if ((d->status & E1000_TXD_STAT_DD) == 0) {
        printk("[E1000] TX desc busy idx=%u status=%x\n", idx, d->status);
        return;
    }

    uint8_t* pkt = &g_tx_bufs[idx][0];
    uint32_t p = 0;

    for (int i = 0; i < 6; i++) pkt[p++] = 0xFF; // broadcast dst

    uint32_t ral = mmio_read32_raw(mmio, E1000_REG_RAL0);
    uint32_t rah = mmio_read32_raw(mmio, E1000_REG_RAH0);
    pkt[p++] = (uint8_t)(ral & 0xFF);
    pkt[p++] = (uint8_t)((ral >> 8) & 0xFF);
    pkt[p++] = (uint8_t)((ral >> 16) & 0xFF);
    pkt[p++] = (uint8_t)((ral >> 24) & 0xFF);
    pkt[p++] = (uint8_t)(rah & 0xFF);
    pkt[p++] = (uint8_t)((rah >> 8) & 0xFF);

    pkt[p++] = 0x88; // EtherType test
    pkt[p++] = 0xB5;

    const char* msg = "KuvixOS e1000 TX test frame";
    for (int i = 0; msg[i] && p < 60; i++) pkt[p++] = (uint8_t)msg[i];
    while (p < 60) pkt[p++] = 0;

    d->length = (uint16_t)p;
    d->cso    = 0;
    d->cmd    = E1000_TXD_CMD_EOP | E1000_TXD_CMD_IFCS | E1000_TXD_CMD_RS;
    d->status = 0;

    uint32_t tdt = (idx + 1) % TX_RING_SIZE;
    mmio_write32_raw(mmio, E1000_REG_TDT, tdt);

    for (int spin = 0; spin < 3000000; spin++) {
        uint8_t st = d->status;
        if (st & E1000_TXD_STAT_DD) break;
    }

    uint8_t st = d->status;
    uint32_t tdh_r = mmio_read32_raw(mmio, E1000_REG_TDH);
    uint32_t tdt_r = mmio_read32_raw(mmio, E1000_REG_TDT);
    uint32_t tctl  = mmio_read32_raw(mmio, E1000_REG_TCTL);

    printk("[E1000] TX sent idx=%u len=%u status=%x kickTDT=%u\n",
           idx, (unsigned)d->length, (unsigned)st, (unsigned)tdt);
    printk("[E1000] TX regs: TDH=%u TDT=%u TCTL=%x\n",
        (unsigned)tdh_r, (unsigned)tdt_r, (unsigned)tctl);

    g_tx_next++;
}

static void e1000_send_arp_request(uint32_t mmio, uint32_t target_ip_be) {
    uint32_t idx = g_tx_next % TX_RING_SIZE;
    volatile e1000_tx_desc_t* d = &g_tx_ring[idx];

    if ((d->status & E1000_TXD_STAT_DD) == 0) {
        printk("[E1000] TX busy (arp) idx=%u\n", idx);
        return;
    }

    uint8_t* pkt = &g_tx_bufs[idx][0];
    uint32_t p = 0;

    // dst MAC = broadcast
    for (int i = 0; i < 6; i++) pkt[p++] = 0xFF;

    // src MAC = RAL/RAH
    uint32_t ral = mmio_read32_raw(mmio, E1000_REG_RAL0);
    uint32_t rah = mmio_read32_raw(mmio, E1000_REG_RAH0);
    uint8_t smac[6] = {
        (uint8_t)(ral & 0xFF),
        (uint8_t)((ral >> 8) & 0xFF),
        (uint8_t)((ral >> 16) & 0xFF),
        (uint8_t)((ral >> 24) & 0xFF),
        (uint8_t)(rah & 0xFF),
        (uint8_t)((rah >> 8) & 0xFF),
    };
    for (int i = 0; i < 6; i++) pkt[p++] = smac[i];

    // EtherType = ARP (0x0806)
    pkt[p++] = 0x08; pkt[p++] = 0x06;

    // ARP header
    // HTYPE=1 (Ethernet)
    pkt[p++] = 0x00; pkt[p++] = 0x01;
    // PTYPE=0x0800 (IPv4)
    pkt[p++] = 0x08; pkt[p++] = 0x00;
    // HLEN=6, PLEN=4
    pkt[p++] = 0x06; pkt[p++] = 0x04;
    // OPER=1 (request)
    pkt[p++] = 0x00; pkt[p++] = 0x01;

    // Sender MAC
    for (int i = 0; i < 6; i++) pkt[p++] = smac[i];

    // Sender IP = 10.0.2.15 (QEMU user-net default)
    pkt[p++] = 0x0A; pkt[p++] = 0x00; pkt[p++] = 0x02; pkt[p++] = 0x0F;

    // Target MAC = 00:00:00:00:00:00
    for (int i = 0; i < 6; i++) pkt[p++] = 0x00;

    // Target IP (big-endian)
    pkt[p++] = (uint8_t)((target_ip_be >> 24) & 0xFF);
    pkt[p++] = (uint8_t)((target_ip_be >> 16) & 0xFF);
    pkt[p++] = (uint8_t)((target_ip_be >> 8) & 0xFF);
    pkt[p++] = (uint8_t)((target_ip_be >> 0) & 0xFF);

    // min frame 60 byte
    while (p < 60) pkt[p++] = 0;

    d->length = (uint16_t)p;
    d->cso    = 0;
    d->cmd    = E1000_TXD_CMD_EOP | E1000_TXD_CMD_IFCS | E1000_TXD_CMD_RS;
    d->status = 0;

    uint32_t tdt = (idx + 1) % TX_RING_SIZE;
    mmio_write32_raw(mmio, E1000_REG_TDT, tdt);

    for (int spin = 0; spin < 3000000; spin++) {
        if (d->status & E1000_TXD_STAT_DD) break;
    }

    printk("[E1000] ARP who-has sent (target=%u.%u.%u.%u) status=%x\n",
           (target_ip_be >> 24) & 0xFF, (target_ip_be >> 16) & 0xFF,
           (target_ip_be >> 8) & 0xFF, target_ip_be & 0xFF,
           (unsigned)d->status);

    g_tx_next++;
}

// ------------------------------------------------------------
// RX init + poll
// ------------------------------------------------------------
static void e1000_rx_init(uint32_t mmio) {
    memset((void*)g_rx_ring, 0, sizeof(g_rx_ring));
    memset((void*)g_rx_bufs, 0, sizeof(g_rx_bufs));

    for (uint32_t i = 0; i < RX_RING_SIZE; i++) {
        g_rx_ring[i].addr = (uint32_t)(uintptr_t)&g_rx_bufs[i][0];
        g_rx_ring[i].status = 0;
    }

    uint32_t ring_phys = (uint32_t)(uintptr_t)g_rx_ring;

    mmio_write32_raw(mmio, E1000_REG_RDBAL, ring_phys);
    mmio_write32_raw(mmio, E1000_REG_RDBAH, 0);
    mmio_write32_raw(mmio, E1000_REG_RDLEN, RX_RING_SIZE * sizeof(e1000_rx_desc_t));
    mmio_write32_raw(mmio, E1000_REG_RDH, 0);
    mmio_write32_raw(mmio, E1000_REG_RDT, RX_RING_SIZE - 1);

    // RCTL ayarla:
    // EN + BAM (broadcast accept) + MPE (multicast accept)
    // UPE (unicast promiscuous) istersen ekleyebilirsin; şimdilik kapalı.
    uint32_t rctl = 0;
    rctl |= E1000_RCTL_EN;
    rctl |= E1000_RCTL_BAM;
    rctl |= E1000_RCTL_MPE;
    rctl |= E1000_RCTL_BSIZE_2048;
    rctl |= E1000_RCTL_UPE;

    mmio_write32_raw(mmio, E1000_REG_RCTL, rctl);

    g_rx_next = 0;

    printk("[E1000] RX init ring=%x bufs=%x RCTL=%x\n",
           ring_phys, (uint32_t)(uintptr_t)g_rx_bufs, rctl);
}

static void e1000_poll_rx(uint32_t mmio) {
    // DD set olan descriptor var mı diye bak
    volatile e1000_rx_desc_t* d = &g_rx_ring[g_rx_next];



    uint8_t st = d->status;
    static uint32_t dbg = 0;
    dbg++;
    if ((dbg % 2000000) == 0) {
        uint32_t rdh = mmio_read32_raw(mmio, E1000_REG_RDH);
        uint32_t rdt = mmio_read32_raw(mmio, E1000_REG_RDT);
        uint32_t rctl = mmio_read32_raw(mmio, E1000_REG_RCTL);
        printk("[E1000][RX] debug RDH=%u RDT=%u next=%u st=%x RCTL=%x\n",
            (unsigned)rdh, (unsigned)rdt, (unsigned)g_rx_next, (unsigned)st, (unsigned)rctl);
    }
    if ((st & E1000_RXD_STAT_DD) == 0) return;

    uint16_t len = d->length;
    uint8_t* pkt = &g_rx_bufs[g_rx_next][0];

    // EtherType oku (14 byte header)
    uint16_t ethertype = 0;
    if (len >= 14) {
        ethertype = ((uint16_t)pkt[12] << 8) | (uint16_t)pkt[13];
    }

    printk("[E1000][RX] idx=%u len=%u status=%x ethertype=0x%x\n",
           (unsigned)g_rx_next, (unsigned)len, (unsigned)st, (unsigned)ethertype);

    // İlk 64 byte dump
    uint32_t dump_n = (len < 64) ? len : 64;
    dump_hex16(pkt, dump_n);

    // Descriptor'ı tekrar boşalt
    d->status = 0;

    // Tail ilerlet: NIC'e "bu descriptor tekrar serbest" de
    mmio_write32_raw(mmio, E1000_REG_RDT, g_rx_next);

    g_rx_next = (g_rx_next + 1) % RX_RING_SIZE;
}

// ------------------------------------------------------------
// Probe entry
// ------------------------------------------------------------
int e1000_probe(uint8_t bus, uint8_t slot, uint8_t func) {
    uint16_t vid = pci_read16(bus, slot, func, 0x00);
    uint16_t did = pci_read16(bus, slot, func, 0x02);

    if (vid != 0x8086) return 0;
    if (did != 0x100E) return 0;

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

    e1000_force_link_up(mmio);

    // TX
    e1000_tx_init(mmio);
    e1000_send_test_frame(mmio);

    // RX
    e1000_rx_init(mmio);
    e1000_send_test_frame(mmio);

    for (int tries = 0; tries < 5; tries++) {
        e1000_send_arp_request(mmio, 0x0A000202); // 10.0.2.2

        // her denemede biraz poll
        for (int spin = 0; spin < 8000000; spin++) {
            e1000_poll_rx(mmio);
        }
    }

    return 1;
}