// kernel/commands/cmd_ipconfig.c
#include <kernel/printk.h>
#include <kernel/drivers/net/net.h>
#include <kernel/drivers/net/e1000.h>
#include <lib/commands.h>
#include <stdint.h>

static void print_ip(uint32_t ip_be) {
    printk("%u.%u.%u.%u",
           (ip_be >> 24) & 0xFF,
           (ip_be >> 16) & 0xFF,
           (ip_be >> 8)  & 0xFF,
           (ip_be >> 0)  & 0xFF);
}

static void print_mac(const uint8_t mac[6]) {
    printk("%x:%x:%x:%x:%x:%x", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

static void cmd_ipconfig(int argc, char** argv) {
    (void)argc; (void)argv;

    uint32_t ip=0, mask=0, gw=0;
    net_get_ipv4(&ip, &mask, &gw);

    printk("KuvixOS Network\n");
    printk("  link   : %s\n", e1000_is_ready() ? "up" : "down");
    printk("  ip     : "); print_ip(ip);   printk("\n");
    printk("  mask   : "); print_ip(mask); printk("\n");
    printk("  gateway: "); print_ip(gw);   printk("\n");

    if (e1000_is_ready()) {
        uint8_t mac[6];
        e1000_get_mac(mac);
        printk("  mac    : ");
        print_mac(mac);
        printk("\n");
    }
}

REGISTER_COMMAND(ipconfig, cmd_ipconfig, "Ag bilgilerini gosterir (ip/mask/gw/mac)");