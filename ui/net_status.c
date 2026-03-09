#include <ui/net_status.h>
#include <kernel/drivers/net/net.h>
#include <kernel/drivers/net/e1000.h>
#include <kernel/printk.h>

static net_state_t g_state = NETS_DOWN;

// basit timer: tick sayacı (desktop_tick çağrı sıklığına göre ayarla)
static int g_counter = 0;
static int g_period  = 600; // ~10 saniye gibi (tick rate'inize göre değiştirin)
static int g_force   = 1;

static void do_check(void) {
    if (!e1000_is_ready()) {
        g_state = NETS_DOWN;
        return;
    }

    // en azından link var diyelim
    g_state = NETS_LINK;

    // gw ve internet ping (MVP)
    uint32_t ip=0, mask=0, gw=0;
    net_get_ipv4(&ip, &mask, &gw);

    if (!gw) return;

    if (!net_ping_ipv4(gw)) {
        g_state = NETS_LINK;
        return;
    }
    g_state = NETS_LAN;

    // internet testi: 1.1.1.1
    if (net_ping_ipv4(0x01010101)) { // 1.1.1.1 big-endian
        g_state = NETS_INET;
    }
}

void net_status_init(void) {
    g_counter = 0;
    g_force = 1;
    g_state = NETS_DOWN;
}

void net_status_tick(void) {
    g_counter++;
    if (g_force || g_counter >= g_period) {
        g_force = 0;
        g_counter = 0;
        do_check();
    }
}

void net_status_force_check(void) { g_force = 1; }

net_state_t net_status_get(void) { return g_state; }

const char* net_status_text(void) {
    switch (g_state) {
        case NETS_DOWN: return "Down";
        case NETS_LINK: return "Link";
        case NETS_LAN:  return "LAN OK";
        case NETS_INET: return "Internet OK";
        default:        return "?";
    }
}