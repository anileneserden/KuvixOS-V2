#include <kernel/printk.h>
#include <kernel/drivers/net/net.h>
#include <lib/commands.h>
#include <lib/string.h>
#include <stdint.h>

static void print_ip_be(uint32_t ip_be) {
    printk("%u.%u.%u.%u",
        (unsigned)((ip_be >> 24) & 0xFF),
        (unsigned)((ip_be >> 16) & 0xFF),
        (unsigned)((ip_be >>  8) & 0xFF),
        (unsigned)( ip_be        & 0xFF));
}

static int parse_ip4_be(const char* s, uint32_t* out_be) {
    if (!s || !out_be) return 0;

    uint32_t parts[4] = {0,0,0,0};
    int part = 0;
    uint32_t val = 0;
    int has_digit = 0;

    for (int i=0;; i++) {
        char c = s[i];
        if (c >= '0' && c <= '9') {
            has_digit = 1;
            val = val * 10 + (uint32_t)(c - '0');
            if (val > 255) return 0;
            continue;
        }
        if (c == '.' || c == 0) {
            if (!has_digit) return 0;
            if (part > 3) return 0;
            parts[part++] = val;
            val = 0;
            has_digit = 0;
            if (c == 0) break;
            continue;
        }
        return 0;
    }

    if (part != 4) return 0;
    *out_be = (parts[0]<<24) | (parts[1]<<16) | (parts[2]<<8) | parts[3];
    return 1;
}

static void cmd_nslookup(int argc, char** argv) {
    if (argc < 2) {
        commands_puts("Kullanim: nslookup <host> [dns_ip]\n");
        commands_puts("Ornek   : nslookup example.com\n");
        commands_puts("Ornek   : nslookup example.com 8.8.8.8\n");
        return;
    }

    const char* host = argv[1];

    // QEMU slirp için sık kullanılan DNS forwarder
    uint32_t dns_ip_be = 0x0A000203; // 10.0.2.3

    if (argc >= 3) {
        if (!parse_ip4_be(argv[2], &dns_ip_be)) {
            commands_puts("DNS IP gecersiz.\n");
            return;
        }
    }

    uint32_t out_ip = 0;
    if (!net_dns_resolve_a(host, dns_ip_be, &out_ip)) {
        printk("DNS resolve basarisiz: %s (dns=", host);
        print_ip_be(dns_ip_be);
        printk(")\n");
        return;
    }

    printk("%s -> ", host);
    print_ip_be(out_ip);
    printk("\n");
}

REGISTER_COMMAND(nslookup, cmd_nslookup, "DNS A kaydi cozer (nslookup <host> [dns_ip])");