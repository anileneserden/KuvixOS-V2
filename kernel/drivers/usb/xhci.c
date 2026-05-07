#include <kernel/drivers/usb/xhci.h>
#include <kernel/drivers/usb/usb.h>
#include <kernel/printk.h>
#include <kernel/memory/kmalloc.h>
#include <lib/string.h>
#include <kernel/drivers/usb/hid.h>

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
    TRB_TYPE_NORMAL = 1,
    TRB_TYPE_SETUP_STAGE = 2,
    TRB_TYPE_DATA_STAGE = 3,
    TRB_TYPE_STATUS_STAGE = 4,
    TRB_TYPE_LINK = 6, 
    TRB_TYPE_ENABLE_SLOT_CMD = 9,
    TRB_TYPE_ADDRESS_DEVICE = 11, 
    TRB_TYPE_CONFIGURE_ENDPOINT_CMD = 12,
    TRB_TYPE_TRANSFER_EVT = 32,
    TRB_TYPE_CMD_COMPLETION_EVT = 33,

    XHCI_PORTSC_CCS = 1u << 0,
    XHCI_PORTSC_PED = 1u << 1,
    XHCI_PORTSC_PR  = 1u << 4,
    XHCI_PORTSC_SPEED_SHIFT = 10,
    XHCI_PORTSC_SPEED_MASK = 0xFu << XHCI_PORTSC_SPEED_SHIFT,
    XHCI_PORTSC_CHANGE_BITS = (1u << 17) | (1u << 18) | (1u << 19) |
                             (1u << 20) | (1u << 21) | (1u << 22) |
                             (1u << 23),

    XHCI_TRB_IOC = 1u << 5,
    XHCI_TRB_IDT = 1u << 6,
    XHCI_TRB_CHAIN = 1u << 4,
    XHCI_TRB_DIR_IN = 1u << 16,
    XHCI_TRB_TRT_NONE = 0u << 16,
    XHCI_TRB_TRT_OUT  = 2u << 16,
    XHCI_TRB_TRT_IN   = 3u << 16,

    XHCI_CC_SUCCESS = 1,
    XHCI_CC_BABBLE_DETECTED = 3,
    XHCI_CC_USB_TRANSACTION_ERROR = 4,
    XHCI_CC_TRB_ERROR = 5,
    XHCI_CC_STALL_ERROR = 6,
    XHCI_CC_SHORT_PACKET = 13,
};

typedef struct {
    xhci_trb_t* trbs;
    uint32_t phys;
    uint32_t cycle;
    uint32_t index;
    uint32_t size;
} xhci_transfer_ring_t;

typedef struct {
    uint32_t type;
    uint32_t completion_code;
    uint32_t slot_id;
    uint32_t endpoint_id;
    uint32_t d0;
    uint32_t d1;
    uint32_t d2;
    uint32_t d3;
} xhci_event_info_t;

typedef struct {
    uint8_t valid;
    uint8_t configuration_value;
    uint8_t interface_number;
    uint8_t subclass;
    uint8_t protocol;
    uint8_t bulk_in_ep;
    uint8_t bulk_out_ep;
    uint16_t bulk_in_mps;
    uint16_t bulk_out_mps;
    uint8_t bulk_in_dci;
    uint8_t bulk_out_dci;
    uint32_t block_size;
    uint32_t block_count;
} xhci_msc_state_t;

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
static uint8_t  g_xhci_port_connected[256] = {0};
static uint32_t g_xhci_dboff = 0;
static uint32_t g_xhci_rtsoff = 0;
static xhci_transfer_ring_t g_ep0_ring = {0};
static xhci_transfer_ring_t g_msc_bulk_in_ring = {0};
static xhci_transfer_ring_t g_msc_bulk_out_ring = {0};
static xhci_msc_state_t g_msc = {0};
static uint32_t g_current_slot = 0;
static uint32_t g_current_port = 0;
static uint32_t g_msc_tag_seed = 0x58484D53;
static uint8_t g_msc_recovery_active = 0;
static blockdev_t g_usb_msc_dev = {0};

static uint32_t xhci_port_speed(uint32_t portsc);
static void xhci_snapshot_port_states(void);
static int xhci_hotplug_enumerate_port(uint32_t port_id);
static void xhci_hotplug_handle_disconnect(uint32_t port_id);
static int xhci_reset_port(uint32_t port_1based);
static int xhci_enable_slot(uint32_t mmio, uint32_t dboff, uint32_t rtsoff);
static int xhci_address_device(uint32_t mmio, uint32_t dboff, uint32_t rtsoff, uint32_t port_id, uint32_t slot);
static int xhci_msc_request_sense(uint32_t mmio, uint32_t dboff, uint32_t rtsoff);
static int xhci_msc_bot_command(uint32_t mmio, uint32_t dboff, uint32_t rtsoff,
    const uint8_t* cdb, uint32_t cdb_len, void* data_buf, uint32_t data_len,
    int dir_in, uint32_t tag, usb_msc_csw_t* out_csw);
static int xhci_msc_read10(uint32_t lba, uint16_t count, void* out);
static int xhci_usb_msc_block_read(blockdev_t* dev, uint64_t lba, uint32_t count, void* out);

static uint32_t xhci_msc_next_tag(void) {
    g_msc_tag_seed++;
    if (g_msc_tag_seed == 0) {
        g_msc_tag_seed = 1;
    }
    return g_msc_tag_seed;
}

static uint32_t xhci_be32_to_cpu(const uint8_t* data) {
    if (!data) {
        return 0;
    }

    return ((uint32_t)data[0] << 24) |
           ((uint32_t)data[1] << 16) |
           ((uint32_t)data[2] << 8) |
           (uint32_t)data[3];
}

static int xhci_usb_ep_is_msc_bulk(uint8_t ep_addr) {
    return ep_addr == g_msc.bulk_in_ep || ep_addr == g_msc.bulk_out_ep;
}

static const char* xhci_completion_code_name(int completion_code) {
    switch (completion_code) {
        case -1: return "timeout";
        case XHCI_CC_SUCCESS: return "success";
        case XHCI_CC_BABBLE_DETECTED: return "babble";
        case XHCI_CC_USB_TRANSACTION_ERROR: return "usb-transaction";
        case XHCI_CC_TRB_ERROR: return "trb-error";
        case XHCI_CC_STALL_ERROR: return "stall";
        case XHCI_CC_SHORT_PACKET: return "short-packet";
        default: return "other";
    }
}

static int xhci_msc_should_recover_completion(int completion_code) {
    switch (completion_code) {
        case -1:
        case XHCI_CC_BABBLE_DETECTED:
        case XHCI_CC_USB_TRANSACTION_ERROR:
        case XHCI_CC_STALL_ERROR:
            return 1;
        default:
            return 0;
    }
}

static uint32_t xhci_dci_from_ep_addr(uint8_t ep_addr) {
    uint32_t ep_num = ep_addr & 0x0F;
    uint32_t is_in = (ep_addr & 0x80) ? 1u : 0u;
    return ep_num ? (ep_num * 2u) + is_in : 1u;
}

static uint32_t* xhci_endpoint_context(xhci_input_context_t* input, uint32_t dci) {
    if (!input || dci == 0 || dci > 31) {
        return 0;
    }

    return &input->dev.ep0[0] + ((dci - 1u) * 8u);
}

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

int xhci_poll_hotplug(void) {
    if (!g_xhci_mmio_base || g_xhci_max_ports == 0) {
        return 0;
    }

    uint32_t op = g_xhci_mmio_base + g_xhci_caplen;

    for (uint32_t port = 1; port <= g_xhci_max_ports && port < 256; port++) {
        uint32_t off = 0x400 + (port - 1) * 0x10;
        uint32_t portsc = mmio_read32(op, off);
        uint8_t connected = (portsc & XHCI_PORTSC_CCS) ? 1u : 0u;
        uint8_t previous = g_xhci_port_connected[port];
        uint32_t changed = portsc & XHCI_PORTSC_CHANGE_BITS;

        if (!changed && connected == previous) {
            continue;
        }

        if (changed) {
            mmio_write32(op, off, portsc | changed);
        }

        if (connected == previous) {
            continue;
        }

        g_xhci_port_connected[port] = connected;

        if (connected) {
            xhci_hotplug_enumerate_port(port);
            printk("[xHCI] Hotplug connect detected on port %d (PED=%d speed=%d).\n",
                port,
                (portsc & XHCI_PORTSC_PED) ? 1 : 0,
                xhci_port_speed(portsc));
            return (int)port;
        }

        xhci_hotplug_handle_disconnect(port);
        printk("[xHCI] Hotplug disconnect detected on port %d.\n", port);
        return -(int)port;
    }

    return 0;
}

int xhci_usb_msc_ready(void) {
    return g_msc.valid && g_msc.block_size != 0 && g_usb_msc_dev.read != 0;
}

blockdev_t* xhci_usb_msc_get_dev(void) {
    return xhci_usb_msc_ready() ? &g_usb_msc_dev : 0;
}

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

static int xhci_transfer_ring_init(xhci_transfer_ring_t* ring, uint32_t trb_count) {
    if (!ring || trb_count < 2) {
        return 0;
    }

    memset(ring, 0, sizeof(*ring));

    ring->trbs = xhci_alloc_aligned(sizeof(xhci_trb_t) * trb_count, 64, &ring->phys);
    if (!ring->trbs) {
        return 0;
    }

    ring->cycle = 1;
    ring->index = 0;
    ring->size = trb_count;

    ring->trbs[trb_count - 1].d0 = ring->phys;
    ring->trbs[trb_count - 1].d1 = 0;
    ring->trbs[trb_count - 1].d2 = 0;
    ring->trbs[trb_count - 1].d3 = (TRB_TYPE_LINK << 10) | (1u << 1) | 1u;
    return 1;
}

static int __attribute__((unused)) xhci_transfer_ring_push(xhci_transfer_ring_t* ring, xhci_trb_t trb) {
    if (!ring || !ring->trbs || ring->size < 2) {
        return 0;
    }

    trb.d3 = (trb.d3 & ~1u) | (ring->cycle & 1u);
    ring->trbs[ring->index++] = trb;

    if (ring->index >= ring->size - 1) {
        ring->trbs[ring->size - 1].d3 = (TRB_TYPE_LINK << 10) | (1u << 1) | (ring->cycle & 1u);
        ring->index = 0;
        ring->cycle ^= 1u;
    }

    return 1;
}

static int xhci_transfer_ring_reset(xhci_transfer_ring_t* ring) {
    if (!ring || !ring->trbs || ring->size < 2) {
        return 0;
    }

    memset(ring->trbs, 0, sizeof(xhci_trb_t) * ring->size);
    ring->cycle = 1;
    ring->index = 0;
    ring->trbs[ring->size - 1].d0 = ring->phys;
    ring->trbs[ring->size - 1].d1 = 0;
    ring->trbs[ring->size - 1].d2 = 0;
    ring->trbs[ring->size - 1].d3 = (TRB_TYPE_LINK << 10) | (1u << 1) | 1u;
    return 1;
}

static int xhci_poll_event(uint32_t mmio, uint32_t rtsoff, uint32_t spins, xhci_event_info_t* out) {
    uint32_t intr0 = mmio + (rtsoff & ~0x1F) + 0x20;

    for (uint32_t spin = 0; spin < spins; spin++) {
        xhci_trb_t* e = &g_evt_ring[g_evt_ring_index];

        // Donanım TRB'yi yazdı mı? (Cycle bit kontrolü)
        if ((e->d3 & 1u) == (g_evt_ring_cycle & 1u)) {
            if (out) {
                out->type = (e->d3 >> 10) & 0x3F;
                out->completion_code = (e->d2 >> 24) & 0xFF;
                out->slot_id = (e->d3 >> 24) & 0xFF;
                out->endpoint_id = (e->d3 >> 16) & 0x1F;
                out->d0 = e->d0;
                out->d1 = e->d1;
                out->d2 = e->d2;
                out->d3 = e->d3;
            }

            g_evt_ring_index++;
            if (g_evt_ring_index >= g_evt_ring_size) {
                g_evt_ring_index = 0;
                g_evt_ring_cycle ^= 1u;
            }

            // ERDP (Event Ring Dequeue Pointer) güncelle ve EHB bitini temizle
            uint32_t erdp_val = g_evt_ring_phys + (g_evt_ring_index * sizeof(xhci_trb_t));
            mmio_write64(intr0, 0x18, erdp_val | 0x8);

            return 1;
        }
        delay(100);
    }
    return 0;
}

static int xhci_poll_cmd_completion(uint32_t mmio, uint32_t rtsoff) {
    xhci_event_info_t evt;

    while (xhci_poll_event(mmio, rtsoff, 5000000, &evt)) {
        if (evt.type == TRB_TYPE_CMD_COMPLETION_EVT) {
            return (int)evt.slot_id;
        }
    }

    return -1;
}

static int xhci_poll_cmd_success(uint32_t mmio, uint32_t rtsoff, uint32_t expected_slot) {
    xhci_event_info_t evt;

    while (xhci_poll_event(mmio, rtsoff, 5000000, &evt)) {
        if (evt.type != TRB_TYPE_CMD_COMPLETION_EVT) {
            continue;
        }

        printk("[xHCI] Cmd Event: cc=%d slot=%d raw=%x:%x:%x:%x\n",
            evt.completion_code,
            evt.slot_id,
            evt.d0,
            evt.d1,
            evt.d2,
            evt.d3);

        if (expected_slot && evt.slot_id != expected_slot) {
            continue;
        }

        return (evt.completion_code == 1) ? 1 : 0;
    }

    printk("[xHCI] Command completion timeout waiting for slot=%d.\n", expected_slot);
    return 0;
}

static int xhci_poll_transfer_completion(uint32_t mmio, uint32_t rtsoff, uint32_t slot_id, uint32_t endpoint_id) {
    xhci_event_info_t evt;

    while (xhci_poll_event(mmio, rtsoff, 5000000, &evt)) {
        printk("[xHCI] Event: type=%d cc=%d slot=%d ep=%d raw=%x:%x:%x:%x\n",
            evt.type,
            evt.completion_code,
            evt.slot_id,
            evt.endpoint_id,
            evt.d0,
            evt.d1,
            evt.d2,
            evt.d3);

        if (evt.type != TRB_TYPE_TRANSFER_EVT) {
            continue;
        }

        if (slot_id && evt.slot_id != slot_id) {
            continue;
        }

        if (endpoint_id && evt.endpoint_id != endpoint_id) {
            continue;
        }

        return (int)evt.completion_code;
    }

    printk("[xHCI] Transfer event timeout waiting for slot=%d ep=%d.\n", slot_id, endpoint_id);
    return -1;
}

static void xhci_ring_doorbell(uint32_t mmio, uint32_t dboff, uint32_t slot_id, uint32_t target) {
    mmio_write32(mmio + dboff, slot_id * 4, target & 0xFF);
}

static int xhci_ep0_control_transfer(uint32_t mmio, uint32_t dboff, uint32_t rtsoff, uint32_t slot_id,
    const usb_setup_packet_t* setup, void* data_buf, uint32_t len, int dir_in) {
    uint32_t phys_buf = 0;
    uint8_t* data = xhci_alloc_aligned(len ? len : 1, 64, &phys_buf);
    if (!data) {
        printk("[xHCI] EP0 control buffer allocation failed.\n");
        return 0;
    }

    if (!dir_in && data_buf && len > 0) {
        memcpy(data, data_buf, len);
    }

    uint32_t setup_d0 = (uint32_t)setup->bmRequestType |
                        ((uint32_t)setup->bRequest << 8) |
                        ((uint32_t)setup->wValue << 16);
    uint32_t setup_d1 = (uint32_t)setup->wIndex |
                        ((uint32_t)setup->wLength << 16);

    xhci_trb_t setup_trb = {
        setup_d0,
        setup_d1,
        8,
        (TRB_TYPE_SETUP_STAGE << 10) | XHCI_TRB_IDT |
            ((len > 0) ? (XHCI_TRB_CHAIN | (dir_in ? XHCI_TRB_TRT_IN : XHCI_TRB_TRT_OUT)) : XHCI_TRB_TRT_NONE)
    };
    xhci_trb_t data_trb = {
        phys_buf,
        0,
        len,
        (TRB_TYPE_DATA_STAGE << 10) | XHCI_TRB_CHAIN | (dir_in ? XHCI_TRB_DIR_IN : 0)
    };
    xhci_trb_t status_trb = {
        0,
        0,
        0,
        (TRB_TYPE_STATUS_STAGE << 10) | XHCI_TRB_IOC | ((len == 0 || !dir_in) ? XHCI_TRB_DIR_IN : 0)
    };

    if (!xhci_transfer_ring_push(&g_ep0_ring, setup_trb)) {
        printk("[xHCI] EP0 ring push failed.\n");
        return 0;
    }

    if (len > 0 && !xhci_transfer_ring_push(&g_ep0_ring, data_trb)) {
        printk("[xHCI] EP0 data TRB push failed.\n");
        return 0;
    }

    if (!xhci_transfer_ring_push(&g_ep0_ring, status_trb)) {
        printk("[xHCI] EP0 status TRB push failed.\n");
        return 0;
    }

    xhci_ring_doorbell(mmio, dboff, slot_id, 1);

    int completion = xhci_poll_transfer_completion(mmio, rtsoff, slot_id, 0);
    if (completion <= 0) {
        printk("[xHCI] EP0 control transfer failed (cc=%d).\n", completion);
        return 0;
    }

    if (dir_in && data_buf && len > 0) {
        memcpy(data_buf, data, len);
    }

    return 1;
}

static int xhci_ep0_control_in(uint32_t mmio, uint32_t dboff, uint32_t rtsoff, uint32_t slot_id,
    const usb_setup_packet_t* setup, void* out_buf, uint32_t len) {
    return xhci_ep0_control_transfer(mmio, dboff, rtsoff, slot_id, setup, out_buf, len, 1);
}

static int xhci_ep0_control_out(uint32_t mmio, uint32_t dboff, uint32_t rtsoff, uint32_t slot_id,
    const usb_setup_packet_t* setup, const void* data_buf, uint32_t len) {
    return xhci_ep0_control_transfer(mmio, dboff, rtsoff, slot_id, setup, (void*)data_buf, len, 0);
}

static int xhci_ep0_clear_endpoint_halt(uint32_t mmio, uint32_t dboff, uint32_t rtsoff,
    uint32_t slot_id, uint8_t ep_addr) {
    usb_setup_packet_t setup = {
        .bmRequestType = 0x02,
        .bRequest = USB_REQ_CLEAR_FEATURE,
        .wValue = USB_FEAT_ENDPOINT_HALT,
        .wIndex = ep_addr,
        .wLength = 0,
    };

    if (!ep_addr) {
        return 0;
    }

    if (!xhci_ep0_control_out(mmio, dboff, rtsoff, slot_id, &setup, 0, 0)) {
        printk("[xHCI] CLEAR_FEATURE(ENDPOINT_HALT) failed for ep=0x%x.\n", ep_addr);
        return 0;
    }

    printk("[xHCI] CLEAR_FEATURE(ENDPOINT_HALT) ok for ep=0x%x.\n", ep_addr);
    return 1;
}

static int xhci_msc_bulk_only_reset(uint32_t mmio, uint32_t dboff, uint32_t rtsoff, uint32_t slot_id) {
    usb_setup_packet_t setup = {
        .bmRequestType = 0x21,
        .bRequest = USB_MSC_REQ_BULK_ONLY_RESET,
        .wValue = 0,
        .wIndex = g_msc.interface_number,
        .wLength = 0,
    };

    if (!g_msc.valid) {
        return 0;
    }

    if (!xhci_ep0_control_out(mmio, dboff, rtsoff, slot_id, &setup, 0, 0)) {
        printk("[xHCI] MSC Bulk-Only Reset failed for if=%d.\n", g_msc.interface_number);
        return 0;
    }

    printk("[xHCI] MSC Bulk-Only Reset ok for if=%d.\n", g_msc.interface_number);
    return 1;
}

static int xhci_msc_recover_bulk_error(uint32_t mmio, uint32_t dboff, uint32_t rtsoff,
    uint32_t slot_id, uint8_t failed_ep_addr, int completion_code) {
    if (!g_msc.valid || !slot_id || !xhci_usb_ep_is_msc_bulk(failed_ep_addr) || g_msc_recovery_active) {
        return 0;
    }

    if (!xhci_msc_should_recover_completion(completion_code)) {
        printk("[xHCI] Skipping MSC BOT recovery for ep=0x%x cc=%d (%s).\n",
            failed_ep_addr,
            completion_code,
            xhci_completion_code_name(completion_code));
        return 0;
    }

    printk("[xHCI] Starting MSC BOT recovery for ep=0x%x cc=%d (%s).\n",
        failed_ep_addr,
        completion_code,
        xhci_completion_code_name(completion_code));

    g_msc_recovery_active = 1;

    if (!xhci_msc_bulk_only_reset(mmio, dboff, rtsoff, slot_id)) {
        printk("[xHCI] MSC BOT recovery aborted: bulk-only reset failed.\n");
        g_msc_recovery_active = 0;
        return 0;
    }

    xhci_ep0_clear_endpoint_halt(mmio, dboff, rtsoff, slot_id, g_msc.bulk_in_ep);
    xhci_ep0_clear_endpoint_halt(mmio, dboff, rtsoff, slot_id, g_msc.bulk_out_ep);

    xhci_transfer_ring_reset(&g_msc_bulk_in_ring);
    xhci_transfer_ring_reset(&g_msc_bulk_out_ring);

    if (!xhci_msc_request_sense(mmio, dboff, rtsoff)) {
        printk("[xHCI] REQUEST SENSE after MSC BOT recovery did not complete.\n");
    }

    printk("[xHCI] MSC BOT recovery complete. Rings reset for bulk endpoints.\n");
    g_msc_recovery_active = 0;
    return 1;
}

static int xhci_bulk_transfer(uint32_t mmio, uint32_t dboff, uint32_t rtsoff, uint32_t slot_id,
    uint8_t ep_addr, xhci_transfer_ring_t* ring, void* buf, uint32_t len, int dir_in) {
    uint32_t dci = xhci_dci_from_ep_addr(ep_addr);
    uint32_t phys = 0;
    void* xfer_buf = xhci_alloc_aligned(len ? len : 1, 64, &phys);
    int retried = 0;
    if (!xfer_buf) {
        printk("[xHCI] Bulk transfer buffer allocation failed.\n");
        return 0;
    }

    if (!dir_in && buf && len > 0) {
        memcpy(xfer_buf, buf, len);
    }

    while (1) {
        xhci_trb_t trb = {
            phys,
            0,
            len,
            (TRB_TYPE_NORMAL << 10) | XHCI_TRB_IOC | (dir_in ? XHCI_TRB_DIR_IN : 0)
        };

        if (!xhci_transfer_ring_push(ring, trb)) {
            printk("[xHCI] Bulk ring push failed for dci=%d.\n", dci);
            return 0;
        }

        xhci_ring_doorbell(mmio, dboff, slot_id, dci);

        int completion = xhci_poll_transfer_completion(mmio, rtsoff, slot_id, dci);
        if (completion == XHCI_CC_SUCCESS) {
            break;
        }

        printk("[xHCI] Bulk transfer failed on dci=%d cc=%d (%s).\n",
            dci,
            completion,
            xhci_completion_code_name(completion));
        if (retried || !xhci_msc_recover_bulk_error(mmio, dboff, rtsoff, slot_id, ep_addr, completion)) {
            return 0;
        }

        retried = 1;
        printk("[xHCI] Retrying bulk transfer once after MSC BOT recovery on ep=0x%x.\n", ep_addr);
    }

    if (dir_in && buf && len > 0) {
        memcpy(buf, xfer_buf, len);
    }

    return 1;
}

static int xhci_configure_msc_endpoints(uint32_t mmio, uint32_t dboff, uint32_t rtsoff) {
    uint32_t portsc = xhci_get_portsc(g_current_port);
    uint32_t speed = xhci_port_speed(portsc);
    uint32_t phys_in = 0;
    xhci_input_context_t* input = 0;
    uint32_t* bulk_out_ctx = 0;
    uint32_t* bulk_in_ctx = 0;

    if (!g_msc.valid || !g_current_slot || !g_current_port) {
        return 0;
    }

    g_msc.bulk_out_dci = (uint8_t)xhci_dci_from_ep_addr(g_msc.bulk_out_ep);
    g_msc.bulk_in_dci = (uint8_t)xhci_dci_from_ep_addr(g_msc.bulk_in_ep);

    if (!g_msc_bulk_out_ring.trbs || !g_msc_bulk_in_ring.trbs) {
        printk("[xHCI] MSC bulk rings are not ready.\n");
        return 0;
    }

    input = xhci_alloc_aligned(sizeof(xhci_input_context_t), 64, &phys_in);
    if (!input) {
        printk("[xHCI] Configure Endpoint input context allocation failed.\n");
        return 0;
    }

    input->add_flags = (1u << 0) | (1u << g_msc.bulk_out_dci) | (1u << g_msc.bulk_in_dci);
    input->dev.d[0] = (speed << 20) | (g_msc.bulk_in_dci << 27);
    input->dev.d[1] = (g_current_port << 16);

    bulk_out_ctx = xhci_endpoint_context(input, g_msc.bulk_out_dci);
    bulk_in_ctx = xhci_endpoint_context(input, g_msc.bulk_in_dci);
    if (!bulk_out_ctx || !bulk_in_ctx) {
        printk("[xHCI] Failed to compute MSC endpoint contexts.\n");
        return 0;
    }

    bulk_out_ctx[0] = 0;
    bulk_out_ctx[1] = (3u << 1) | (2u << 3) | ((uint32_t)g_msc.bulk_out_mps << 16);
    bulk_out_ctx[2] = g_msc_bulk_out_ring.phys | 1u;
    bulk_out_ctx[3] = 0;
    bulk_out_ctx[4] = g_msc.bulk_out_mps;

    bulk_in_ctx[0] = 0;
    bulk_in_ctx[1] = (3u << 1) | (6u << 3) | ((uint32_t)g_msc.bulk_in_mps << 16);
    bulk_in_ctx[2] = g_msc_bulk_in_ring.phys | 1u;
    bulk_in_ctx[3] = 0;
    bulk_in_ctx[4] = g_msc.bulk_in_mps;

    xhci_trb_t trb = { phys_in, 0, 0, (g_current_slot << 24) | (TRB_TYPE_CONFIGURE_ENDPOINT_CMD << 10) };
    xhci_cmd_push(trb);
    mmio_write32(mmio + dboff, 0, 0);

    if (!xhci_poll_cmd_success(mmio, rtsoff, g_current_slot)) {
        printk("[xHCI] Configure Endpoint failed for slot %d.\n", g_current_slot);
        return 0;
    }

    printk("[xHCI] Configure Endpoint success: bulk_out_dci=%d bulk_in_dci=%d\n",
        g_msc.bulk_out_dci,
        g_msc.bulk_in_dci);
    return 1;
}

static int xhci_msc_bot_inquiry(uint32_t mmio, uint32_t dboff, uint32_t rtsoff) {
    uint8_t inquiry[36];
    char vendor[9];
    char product[17];
    char revision[5];

    memset(vendor, 0, sizeof(vendor));
    memset(product, 0, sizeof(product));
    memset(revision, 0, sizeof(revision));

    if (!g_msc.valid || !g_msc.bulk_in_ep || !g_msc.bulk_out_ep) {
        return 0;
    }

    memset(inquiry, 0, sizeof(inquiry));

    if (!xhci_transfer_ring_reset(&g_msc_bulk_out_ring) || !xhci_transfer_ring_reset(&g_msc_bulk_in_ring)) {
        printk("[xHCI] MSC BOT ring reset failed before INQUIRY.\n");
        return 0;
    }

    uint8_t inquiry_cdb[6] = { 0x12, 0, 0, 0, sizeof(inquiry), 0 };
    usb_msc_csw_t csw;
    memset(&csw, 0, sizeof(csw));

    if (!xhci_msc_bot_command(mmio, dboff, rtsoff, inquiry_cdb, sizeof(inquiry_cdb),
            inquiry, sizeof(inquiry), 1, xhci_msc_next_tag(), &csw)) {
        printk("[xHCI] MSC INQUIRY command failed.\n");
        return 0;
    }

    memcpy(vendor, &inquiry[8], 8);
    memcpy(product, &inquiry[16], 16);
    memcpy(revision, &inquiry[32], 4);

    printk("[xHCI] MSC INQUIRY ok: peripheral=%x removable=%d vendor='%s' product='%s' rev='%s' csw_status=%d residue=%x\n",
        inquiry[0] & 0x1F,
        (inquiry[1] & 0x80) ? 1 : 0,
        vendor,
        product,
        revision,
        csw.bCSWStatus,
        csw.dCSWDataResidue);
    return 1;
}

static int xhci_msc_test_unit_ready(uint32_t mmio, uint32_t dboff, uint32_t rtsoff) {
    uint8_t cdb[6];
    usb_msc_csw_t csw;

    if (!g_msc.valid || !g_msc.bulk_in_ep || !g_msc.bulk_out_ep) {
        return 0;
    }

    memset(cdb, 0, sizeof(cdb));
    memset(&csw, 0, sizeof(csw));

    cdb[0] = 0x00;

    if (!xhci_msc_bot_command(mmio, dboff, rtsoff, cdb, sizeof(cdb), 0, 0, 0, xhci_msc_next_tag(), &csw)) {
        printk("[xHCI] MSC TEST UNIT READY failed.\n");
        return 0;
    }

    printk("[xHCI] MSC TEST UNIT READY ok: csw_status=%d residue=%x\n",
        csw.bCSWStatus,
        csw.dCSWDataResidue);
    return 1;
}

static int xhci_msc_request_sense(uint32_t mmio, uint32_t dboff, uint32_t rtsoff) {
    uint8_t sense[18];
    uint8_t cdb[6];
    usb_msc_csw_t csw;
    uint8_t response_code = 0;
    uint8_t sense_key = 0;
    uint8_t asc = 0;
    uint8_t ascq = 0;

    if (!g_msc.valid || !g_msc.bulk_in_ep || !g_msc.bulk_out_ep) {
        return 0;
    }

    memset(sense, 0, sizeof(sense));
    memset(cdb, 0, sizeof(cdb));
    memset(&csw, 0, sizeof(csw));

    cdb[0] = 0x03;
    cdb[4] = sizeof(sense);

    if (!xhci_msc_bot_command(mmio, dboff, rtsoff, cdb, sizeof(cdb), sense, sizeof(sense), 1, xhci_msc_next_tag(), &csw)) {
        printk("[xHCI] MSC REQUEST SENSE failed.\n");
        return 0;
    }

    response_code = sense[0] & 0x7F;
    sense_key = sense[2] & 0x0F;
    asc = sense[12];
    ascq = sense[13];

    printk("[xHCI] MSC REQUEST SENSE ok: response=%x sense_key=%x asc=%x ascq=%x csw_status=%d residue=%x\n",
        response_code,
        sense_key,
        asc,
        ascq,
        csw.bCSWStatus,
        csw.dCSWDataResidue);
    return 1;
}

static void xhci_msc_log_auto_sense(uint32_t mmio, uint32_t dboff, uint32_t rtsoff, uint8_t failed_opcode) {
    if (failed_opcode == 0x03) {
        return;
    }

    printk("[xHCI] Attempting REQUEST SENSE after opcode=%x failure.\n", failed_opcode);
    if (!xhci_msc_request_sense(mmio, dboff, rtsoff)) {
        printk("[xHCI] REQUEST SENSE follow-up failed after opcode=%x.\n", failed_opcode);
    }
}

static int xhci_msc_bot_command(uint32_t mmio, uint32_t dboff, uint32_t rtsoff,
    const uint8_t* cdb, uint32_t cdb_len, void* data_buf, uint32_t data_len,
    int dir_in, uint32_t tag, usb_msc_csw_t* out_csw) {
    usb_msc_cbw_t cbw;
    usb_msc_csw_t csw;

    if (!g_msc.valid || !g_msc.bulk_in_ep || !g_msc.bulk_out_ep || !cdb || cdb_len == 0 || cdb_len > 16) {
        return 0;
    }

    memset(&cbw, 0, sizeof(cbw));
    memset(&csw, 0, sizeof(csw));

    cbw.dCBWSignature = 0x43425355;
    cbw.dCBWTag = tag;
    cbw.dCBWDataTransferLength = data_len;
    cbw.bmCBWFlags = dir_in ? 0x80 : 0x00;
    cbw.bCBWLUN = 0;
    cbw.bCBWCBLength = (uint8_t)cdb_len;
    memcpy(cbw.CBWCB, cdb, cdb_len);

    if (!xhci_bulk_transfer(mmio, dboff, rtsoff, g_current_slot, g_msc.bulk_out_ep,
            &g_msc_bulk_out_ring, &cbw, sizeof(cbw), 0)) {
        printk("[xHCI] MSC CBW OUT failed for opcode=%x.\n", cdb[0]);
        return 0;
    }

    if (data_len > 0) {
        if (dir_in) {
            if (!xhci_bulk_transfer(mmio, dboff, rtsoff, g_current_slot, g_msc.bulk_in_ep,
                    &g_msc_bulk_in_ring, data_buf, data_len, 1)) {
                printk("[xHCI] MSC data IN failed for opcode=%x.\n", cdb[0]);
                return 0;
            }
        } else {
            if (!xhci_bulk_transfer(mmio, dboff, rtsoff, g_current_slot, g_msc.bulk_out_ep,
                    &g_msc_bulk_out_ring, data_buf, data_len, 0)) {
                printk("[xHCI] MSC data OUT failed for opcode=%x.\n", cdb[0]);
                return 0;
            }
        }
    }

    if (!xhci_bulk_transfer(mmio, dboff, rtsoff, g_current_slot, g_msc.bulk_in_ep,
            &g_msc_bulk_in_ring, &csw, sizeof(csw), 1)) {
        printk("[xHCI] MSC CSW IN failed for opcode=%x.\n", cdb[0]);
        return 0;
    }

    if (csw.dCSWSignature != 0x53425355) {
        printk("[xHCI] MSC CSW signature mismatch for opcode=%x: %x\n", cdb[0], csw.dCSWSignature);
        return 0;
    }

    if (csw.dCSWTag != tag) {
        printk("[xHCI] MSC CSW tag mismatch for opcode=%x: got=%x expected=%x\n", cdb[0], csw.dCSWTag, tag);
        return 0;
    }

    if (out_csw) {
        memcpy(out_csw, &csw, sizeof(csw));
    }

    if (csw.bCSWStatus != 0) {
        printk("[xHCI] MSC CSW reports failure for opcode=%x status=%d residue=%x\n",
            cdb[0],
            csw.bCSWStatus,
            csw.dCSWDataResidue);
        xhci_msc_log_auto_sense(mmio, dboff, rtsoff, cdb[0]);
        return 0;
    }

    return 1;
}

static int xhci_msc_read_capacity10(uint32_t mmio, uint32_t dboff, uint32_t rtsoff) {
    uint8_t capacity[8];
    uint8_t cdb[10];
    usb_msc_csw_t csw;
    uint32_t last_lba = 0;
    uint32_t block_size = 0;

    if (!g_msc.valid || !g_msc.bulk_in_ep || !g_msc.bulk_out_ep) {
        return 0;
    }

    memset(capacity, 0, sizeof(capacity));
    memset(cdb, 0, sizeof(cdb));
    memset(&csw, 0, sizeof(csw));

    cdb[0] = 0x25;

    if (!xhci_msc_bot_command(mmio, dboff, rtsoff, cdb, sizeof(cdb), capacity, sizeof(capacity), 1, xhci_msc_next_tag(), &csw)) {
        printk("[xHCI] MSC READ CAPACITY(10) failed.\n");
        return 0;
    }

    last_lba = xhci_be32_to_cpu(&capacity[0]);
    block_size = xhci_be32_to_cpu(&capacity[4]);
    g_msc.block_size = block_size;
    g_msc.block_count = last_lba + 1u;

    printk("[xHCI] MSC READ CAPACITY ok: blocks=%u block_size=%u last_lba=%u\n",
        g_msc.block_count,
        g_msc.block_size,
        last_lba);

    g_usb_msc_dev.sector_size = g_msc.block_size;
    g_usb_msc_dev.user = 0;
    g_usb_msc_dev.read = xhci_usb_msc_block_read;
    g_usb_msc_dev.write = 0;

    printk("[xHCI] USB MSC block device is ready for sector reads.\n");
    return 1;
}

static int xhci_msc_read10(uint32_t lba, uint16_t count, void* out) {
    uint8_t cdb[10];
    uint32_t total_len = 0;

    if (!out || !g_msc.block_size || count == 0) {
        return 0;
    }

    if (!g_xhci_mmio_base || !g_xhci_dboff || !g_xhci_rtsoff) {
        return 0;
    }

    memset(cdb, 0, sizeof(cdb));
    cdb[0] = 0x28;
    cdb[2] = (uint8_t)((lba >> 24) & 0xFF);
    cdb[3] = (uint8_t)((lba >> 16) & 0xFF);
    cdb[4] = (uint8_t)((lba >> 8) & 0xFF);
    cdb[5] = (uint8_t)(lba & 0xFF);
    cdb[7] = (uint8_t)((count >> 8) & 0xFF);
    cdb[8] = (uint8_t)(count & 0xFF);

    total_len = count * g_msc.block_size;
    return xhci_msc_bot_command(g_xhci_mmio_base, g_xhci_dboff, g_xhci_rtsoff,
        cdb, sizeof(cdb), out, total_len, 1, xhci_msc_next_tag(), 0);
}

static int xhci_usb_msc_block_read(blockdev_t* dev, uint64_t lba, uint32_t count, void* out) {
    uint8_t* dst = (uint8_t*)out;

    if (!dev || dev != &g_usb_msc_dev || !out || count == 0) {
        return 0;
    }

    for (uint32_t i = 0; i < count; i++) {
        if (!xhci_msc_read10((uint32_t)(lba + i), 1, dst + (i * g_msc.block_size))) {
            printk("[xHCI] USB MSC sector read failed at lba=%u.\n", (uint32_t)(lba + i));
            return 0;
        }
    }

    return 1;
}

static void xhci_msc_reset_state(void) {
    memset(&g_msc, 0, sizeof(g_msc));
    memset(&g_msc_bulk_in_ring, 0, sizeof(g_msc_bulk_in_ring));
    memset(&g_msc_bulk_out_ring, 0, sizeof(g_msc_bulk_out_ring));
    memset(&g_usb_msc_dev, 0, sizeof(g_usb_msc_dev));
}

static void xhci_msc_prepare_bot(void) {
    if (!g_msc.valid) {
        return;
    }

    if (g_msc.bulk_in_ep && !g_msc_bulk_in_ring.trbs) {
        if (!xhci_transfer_ring_init(&g_msc_bulk_in_ring, 64)) {
            printk("[xHCI] MSC bulk-in ring allocation failed.\n");
        }
    }

    if (g_msc.bulk_out_ep && !g_msc_bulk_out_ring.trbs) {
        if (!xhci_transfer_ring_init(&g_msc_bulk_out_ring, 64)) {
            printk("[xHCI] MSC bulk-out ring allocation failed.\n");
        }
    }

    printk("[xHCI] MSC BOT scaffold ready: if=%d subclass=%x proto=%x bulk_in=0x%x mps=%d bulk_out=0x%x mps=%d\n",
        g_msc.interface_number,
        g_msc.subclass,
        g_msc.protocol,
        g_msc.bulk_in_ep,
        g_msc.bulk_in_mps,
        g_msc.bulk_out_ep,
        g_msc.bulk_out_mps);
}

static uint8_t xhci_usb_ep_transfer_type(const usb_endpoint_descriptor_t* ep) {
    return ep ? (ep->bmAttributes & 0x3) : 0;
}

static uint8_t xhci_usb_ep_is_in(const usb_endpoint_descriptor_t* ep) {
    return ep ? ((ep->bEndpointAddress & 0x80) != 0) : 0;
}

static void xhci_log_classification(const usb_device_descriptor_t* device_desc, const uint8_t* config_buf, uint32_t config_len) {
    if (!device_desc) {
        return;
    }

    xhci_msc_reset_state();

    if (device_desc->bDeviceClass == USB_CLASS_HID) {
        printk("[xHCI] Device classified as HID (device class).\n");
        return;
    }

    if (device_desc->bDeviceClass == USB_CLASS_MASS_STORAGE) {
        printk("[xHCI] Device classified as Mass Storage (device class).\n");
        return;
    }

    uint32_t offset = 0;
    while (offset + sizeof(usb_descriptor_header_t) <= config_len) {
        const usb_descriptor_header_t* header = (const usb_descriptor_header_t*)(config_buf + offset);
        if (header->bLength == 0) {
            break;
        }

        if (offset + header->bLength > config_len) {
            break;
        }

        if (header->bDescriptorType == USB_DESC_INTERFACE && header->bLength >= sizeof(usb_interface_descriptor_t)) {
            const usb_interface_descriptor_t* iface = (const usb_interface_descriptor_t*)header;

            if (iface->bInterfaceClass == USB_CLASS_HID) {
                printk("[xHCI] Device classified as HID (interface %d).\n", iface->bInterfaceNumber);
                // HID aygıtı için probe fonksiyonunu çağır
                usb_hid_probe((struct usb_device*)device_desc, iface->bInterfaceNumber, iface->bInterfaceProtocol, iface->bInterfaceSubClass);
                return;
            }

            if (iface->bInterfaceClass == USB_CLASS_MASS_STORAGE) {
                printk("[xHCI] Device classified as Mass Storage (interface %d).\n", iface->bInterfaceNumber);

                g_msc.valid = 1;
                g_msc.interface_number = iface->bInterfaceNumber;
                g_msc.subclass = iface->bInterfaceSubClass;
                g_msc.protocol = iface->bInterfaceProtocol;

                uint32_t ep_offset = offset + header->bLength;
                while (ep_offset + sizeof(usb_descriptor_header_t) <= config_len) {
                    const usb_descriptor_header_t* next = (const usb_descriptor_header_t*)(config_buf + ep_offset);
                    if (next->bLength == 0) {
                        break;
                    }
                    if (ep_offset + next->bLength > config_len) {
                        break;
                    }
                    if (next->bDescriptorType == USB_DESC_INTERFACE) {
                        break;
                    }

                    if (next->bDescriptorType == USB_DESC_ENDPOINT && next->bLength >= sizeof(usb_endpoint_descriptor_t)) {
                        const usb_endpoint_descriptor_t* ep = (const usb_endpoint_descriptor_t*)next;
                        if (xhci_usb_ep_transfer_type(ep) == USB_EP_ATTR_BULK) {
                            if (xhci_usb_ep_is_in(ep)) {
                                g_msc.bulk_in_ep = ep->bEndpointAddress;
                                g_msc.bulk_in_mps = ep->wMaxPacketSize;
                            } else {
                                g_msc.bulk_out_ep = ep->bEndpointAddress;
                                g_msc.bulk_out_mps = ep->wMaxPacketSize;
                            }
                        }
                    }

                    ep_offset += next->bLength;
                }

                xhci_msc_prepare_bot();
                return;
            }

            printk("[xHCI] Interface %d class=%x subclass=%x proto=%x.\n",
                iface->bInterfaceNumber,
                iface->bInterfaceClass,
                iface->bInterfaceSubClass,
                iface->bInterfaceProtocol);
        }

        offset += header->bLength;
    }

    printk("[xHCI] Device class could not be mapped to HID/MSC yet.\n");
}

static int xhci_ep0_get_device_descriptor(uint32_t mmio, uint32_t dboff, uint32_t rtsoff, uint32_t slot_id, usb_device_descriptor_t* out_desc) {
    usb_device_descriptor_t desc;
    usb_setup_packet_t setup = {
        .bmRequestType = 0x80,
        .bRequest = USB_REQ_GET_DESCRIPTOR,
        .wValue = (uint16_t)(USB_DESC_DEVICE << 8),
        .wIndex = 0,
        .wLength = sizeof(desc),
    };

    if (!xhci_ep0_control_in(mmio, dboff, rtsoff, slot_id, &setup, &desc, sizeof(desc))) {
        printk("[xHCI] GET_DESCRIPTOR(device) failed.\n");
        return 0;
    }

    if (out_desc) {
        memcpy(out_desc, &desc, sizeof(desc));
    }

    printk("[xHCI] Device Descriptor: vid=%x pid=%x class=%x subclass=%x proto=%x cfgs=%d\n",
        desc.idVendor,
        desc.idProduct,
        desc.bDeviceClass,
        desc.bDeviceSubClass,
        desc.bDeviceProtocol,
        desc.bNumConfigurations);
    return 1;
}

static int xhci_ep0_get_configuration_and_classify(uint32_t mmio, uint32_t dboff, uint32_t rtsoff,
    uint32_t slot_id, const usb_device_descriptor_t* device_desc) {
    usb_configuration_descriptor_t cfg;
    usb_setup_packet_t setup = {
        .bmRequestType = 0x80,
        .bRequest = USB_REQ_GET_DESCRIPTOR,
        .wValue = (uint16_t)(USB_DESC_CONFIGURATION << 8),
        .wIndex = 0,
        .wLength = sizeof(cfg),
    };

    if (!xhci_ep0_control_in(mmio, dboff, rtsoff, slot_id, &setup, &cfg, sizeof(cfg))) {
        printk("[xHCI] GET_DESCRIPTOR(config header) failed.\n");
        return 0;
    }

    printk("[xHCI] Config Header: total_len=%d interfaces=%d value=%d\n",
        cfg.wTotalLength,
        cfg.bNumInterfaces,
        cfg.bConfigurationValue);

    if (cfg.wTotalLength < sizeof(cfg) || cfg.wTotalLength > 512) {
        printk("[xHCI] Config total length looks invalid: %d\n", cfg.wTotalLength);
        return 0;
    }

    uint32_t phys_cfg = 0;
    uint8_t* full_cfg = xhci_alloc_aligned(cfg.wTotalLength, 64, &phys_cfg);
    if (!full_cfg) {
        printk("[xHCI] Full config buffer allocation failed.\n");
        return 0;
    }

    setup.wLength = cfg.wTotalLength;
    if (!xhci_ep0_control_in(mmio, dboff, rtsoff, slot_id, &setup, full_cfg, cfg.wTotalLength)) {
        printk("[xHCI] GET_DESCRIPTOR(full config) failed.\n");
        return 0;
    }

    xhci_log_classification(device_desc, full_cfg, cfg.wTotalLength);
    g_msc.configuration_value = cfg.bConfigurationValue;
    return 1;
}

static int xhci_ep0_set_configuration(uint32_t mmio, uint32_t dboff, uint32_t rtsoff,
    uint32_t slot_id, uint8_t configuration_value) {
    usb_setup_packet_t setup = {
        .bmRequestType = 0x00,
        .bRequest = USB_REQ_SET_CONFIGURATION,
        .wValue = configuration_value,
        .wIndex = 0,
        .wLength = 0,
    };

    if (!configuration_value) {
        printk("[xHCI] Refusing SET_CONFIGURATION with value 0.\n");
        return 0;
    }

    if (!xhci_ep0_control_out(mmio, dboff, rtsoff, slot_id, &setup, 0, 0)) {
        printk("[xHCI] SET_CONFIGURATION(%d) failed.\n", configuration_value);
        return 0;
    }

    printk("[xHCI] SET_CONFIGURATION ok: value=%d\n", configuration_value);
    return 1;
}

static uint32_t xhci_port_speed(uint32_t portsc) {
    return (portsc & XHCI_PORTSC_SPEED_MASK) >> XHCI_PORTSC_SPEED_SHIFT;
}

static void xhci_snapshot_port_states(void) {
    memset(g_xhci_port_connected, 0, sizeof(g_xhci_port_connected));

    if (!g_xhci_mmio_base || g_xhci_max_ports == 0) {
        return;
    }

    for (uint32_t port = 1; port <= g_xhci_max_ports && port < 256; port++) {
        g_xhci_port_connected[port] = (xhci_get_portsc(port) & XHCI_PORTSC_CCS) ? 1u : 0u;
    }
}

static uint32_t xhci_ep0_max_packet_size(uint32_t speed) {
    switch (speed) {
        case 1: return 8;   // Full-speed fallback
        case 2: return 8;   // Low-speed
        case 3: return 64;  // High-speed
        case 4: return 512; // SuperSpeed
        default: return 64;
    }
}

static int xhci_find_connected_port(void) {
    for (uint32_t port = 1; port <= g_xhci_max_ports; port++) {
        uint32_t portsc = xhci_get_portsc(port);
        if (portsc & XHCI_PORTSC_CCS) {
            printk("[xHCI] Port %d connected (PED=%d speed=%d).\n",
                port,
                (portsc & XHCI_PORTSC_PED) ? 1 : 0,
                xhci_port_speed(portsc));
            return (int)port;
        }
    }

    return 0;
}

static int xhci_hotplug_enumerate_port(uint32_t port_id) {
    int slot = 0;

    if (!g_xhci_mmio_base || !g_xhci_dboff || !g_xhci_rtsoff || port_id == 0) {
        return 0;
    }

    if (!xhci_reset_port(port_id)) {
        printk("[xHCI] Hotplug port %d reset failed.\n", port_id);
        return 0;
    }

    slot = xhci_enable_slot(g_xhci_mmio_base, g_xhci_dboff, g_xhci_rtsoff);
    if (slot <= 0) {
        printk("[xHCI] Hotplug enable slot failed on port %d (result=%d).\n", port_id, slot);
        return 0;
    }

    if (!xhci_address_device(g_xhci_mmio_base, g_xhci_dboff, g_xhci_rtsoff, port_id, (uint32_t)slot)) {
        printk("[xHCI] Hotplug address device failed on port %d.\n", port_id);
        return 0;
    }

    return 1;
}

static void xhci_hotplug_handle_disconnect(uint32_t port_id) {
    if (g_current_port == port_id) {
        xhci_msc_reset_state();
        g_current_slot = 0;
        g_current_port = 0;
    }
}

static int xhci_reset_port(uint32_t port_1based) {
    if (!g_xhci_mmio_base || port_1based == 0) {
        return 0;
    }

    uint32_t op = g_xhci_mmio_base + g_xhci_caplen;
    uint32_t off = 0x400 + (port_1based - 1) * 0x10;
    uint32_t portsc = mmio_read32(op, off);

    if (!(portsc & XHCI_PORTSC_CCS)) {
        printk("[xHCI] Port %d reset skipped: no device connected.\n", port_1based);
        return 0;
    }

    printk("[xHCI] Resetting port %d...\n", port_1based);
    mmio_write32(op, off, (portsc & ~XHCI_PORTSC_CHANGE_BITS) | XHCI_PORTSC_PR);

    for (uint32_t spin = 0; spin < 2000000; spin++) {
        uint32_t current = mmio_read32(op, off);
        if (!(current & XHCI_PORTSC_PR)) {
            printk("[xHCI] Port %d reset complete (PED=%d speed=%d).\n",
                port_1based,
                (current & XHCI_PORTSC_PED) ? 1 : 0,
                xhci_port_speed(current));
            return 1;
        }
        delay(100);
    }

    printk("[xHCI] Port %d reset timeout.\n", port_1based);
    return 0;
}

static int xhci_controller_init(uint32_t mmio, uint32_t* out_dboff, uint32_t* out_rtsoff) {
    uint32_t dboff = mmio_read32(mmio, 0x14);
    uint32_t rtsoff = mmio_read32(mmio, 0x18);
    uint32_t op = mmio + g_xhci_caplen;

    if (out_dboff) *out_dboff = dboff;
    if (out_rtsoff) *out_rtsoff = rtsoff;
    g_xhci_dboff = dboff;
    g_xhci_rtsoff = rtsoff;

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

    return 1;
}

static int xhci_enable_slot(uint32_t mmio, uint32_t dboff, uint32_t rtsoff) {
    g_evt_ring_cycle = 1;
    g_evt_ring_index = 0;

    printk("[xHCI] Sending Enable Slot command...\n");
    xhci_trb_t t = {0, 0, 0, (9u << 10)}; // TRB_TYPE_ENABLE_SLOT_CMD = 9
    xhci_cmd_push(t);
    
    // Doorbell 0'ı çal
    mmio_write32(mmio + dboff, 0, 0); 

    return xhci_poll_cmd_completion(mmio, rtsoff);
}

static int xhci_address_device(uint32_t mmio, uint32_t dboff, uint32_t rtsoff, uint32_t port_id, uint32_t slot) {
    uint32_t portsc = xhci_get_portsc(port_id);
    uint32_t speed = xhci_port_speed(portsc);
    uint32_t max_packet = xhci_ep0_max_packet_size(speed);

    if (slot <= 0) {
        return 0;
    }

    printk("[xHCI] SUCCESS! Slot assigned: %d. Addressing device...\n", slot);

    // 1. Input Context Hazırla (Cihaz özelliklerini bildirmek için)
    uint32_t phys_in = 0;
    xhci_input_context_t* input = xhci_alloc_aligned(sizeof(xhci_input_context_t), 64, &phys_in);
    if (!input) {
        printk("[xHCI] Input Context allocation failed.\n");
        return 0;
    }

    // Slot ve Endpoint 0'ı konfigüre edeceğimizi belirtiyoruz
    input->add_flags = 0x03;

    if (!xhci_transfer_ring_init(&g_ep0_ring, 64)) {
        printk("[xHCI] EP0 transfer ring allocation failed.\n");
        return 0;
    }

    // Slot Context: context entries + port speed
    input->dev.d[0] = (speed << 20) | (1u << 27);
    input->dev.d[1] = (port_id << 16); // Root Hub Port Number

    // Endpoint 0 Context:
    //   d0: interval/state alanlari - EP0 icin sifir yeterli
    //   d1: error count + endpoint type(control) + max packet size
    //   d2/d3: TR Dequeue Pointer + DCS
    //   d4: average TRB length
    input->dev.ep0[0] = 0;
    input->dev.ep0[1] = (3u << 1) | (4u << 3) | (max_packet << 16);
    input->dev.ep0[2] = g_ep0_ring.phys | 1u;
    input->dev.ep0[3] = 0;
    input->dev.ep0[4] = 8;

    printk("[xHCI] Programming slot %d on port %d speed=%d ep0_mps=%d.\n",
        slot, port_id, speed, max_packet);

    // 2. Device Context (Donanımın kendi yazacağı alan)
    uint32_t phys_ctx = 0;
    if (!xhci_alloc_aligned(sizeof(xhci_device_context_t), 64, &phys_ctx)) {
        printk("[xHCI] Device Context allocation failed.\n");
        return 0;
    }

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
        g_current_slot = slot;
        g_current_port = port_id;
        printk("[xHCI] EP0 ring prepared at %x for slot %d port %d.\n",
            g_ep0_ring.phys, slot, port_id);
        printk("[xHCI] ADDRESS SUCCESS! Device is now in ADDRESSED state.\n");

        usb_device_descriptor_t device_desc;
        memset(&device_desc, 0, sizeof(device_desc));
        if (!xhci_ep0_get_device_descriptor(mmio, dboff, rtsoff, slot, &device_desc)) {
            printk("[xHCI] Device descriptor read did not complete yet.\n");
        } else {
            if (xhci_ep0_get_configuration_and_classify(mmio, dboff, rtsoff, slot, &device_desc) && g_msc.valid) {
                if (xhci_ep0_set_configuration(mmio, dboff, rtsoff, slot, g_msc.configuration_value)) {
                    if (xhci_configure_msc_endpoints(mmio, dboff, rtsoff)) {
                        if (xhci_msc_bot_inquiry(mmio, dboff, rtsoff)) {
                            if (xhci_msc_test_unit_ready(mmio, dboff, rtsoff)) {
                                if (!xhci_msc_request_sense(mmio, dboff, rtsoff)) {
                                    printk("[xHCI] REQUEST SENSE probe did not complete; continuing with READ CAPACITY.\n");
                                }
                                xhci_msc_read_capacity10(mmio, dboff, rtsoff);
                            }
                        }
                    }
                }
            }
        }
        return 1;
    }

    printk("[xHCI] Address Device failed/timeout.\n");
    return 0;
}

void xhci_minimal_init(uint32_t mmio) {
    uint32_t dboff = 0;
    uint32_t rtsoff = 0;
    int port_id = 0;

    printk("[xHCI] Starting Minimal Init at %x...\n", mmio);
    xhci_set_global(mmio);
    xhci_snapshot_port_states();

    if (!xhci_controller_init(mmio, &dboff, &rtsoff)) {
        printk("[xHCI] Controller init failed.\n");
        return;
    }

    xhci_snapshot_port_states();

    port_id = xhci_find_connected_port();
    if (port_id <= 0) {
        printk("[xHCI] No connected ports found.\n");
        return;
    }

    if (!xhci_reset_port((uint32_t)port_id)) {
        printk("[xHCI] Port %d reset failed, continuing anyway.\n", port_id);
    }

    int slot = xhci_enable_slot(mmio, dboff, rtsoff);
    if (slot <= 0) {
        printk("[xHCI] FAILED: Could not enable slot (result: %d)\n", slot);
        return;
    }

    xhci_address_device(mmio, dboff, rtsoff, (uint32_t)port_id, (uint32_t)slot);
}