// kernel/commands/cmd_ping.c
#include <kernel/printk.h>
#include <kernel/drivers/net/net.h>
#include <lib/commands.h>
#include <stdint.h>

// "a.b.c.d" -> big-endian u32 (sscanf yok)
static int parse_ip4_be(const char* s, uint32_t* out_be) {
    if (!s || !out_be) return 0;

    uint32_t parts[4] = {0,0,0,0};
    int part = 0;
    uint32_t val = 0;
    int has_digit = 0;

    for (int i = 0;; i++) {
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

    *out_be = (parts[0] << 24) | (parts[1] << 16) | (parts[2] << 8) | parts[3];
    return 1;
}

static void cmd_ping(int argc, char** argv) {
    if (argc < 2 || !argv || !argv[1]) {
        commands_puts("Kullanim: ping <ip>\n");
        commands_puts("Ornek: ping 10.0.2.2\n");
        return;
    }

    uint32_t ip_be = 0;
    if (!parse_ip4_be(argv[1], &ip_be)) {
        commands_puts("Gecersiz IP.\n");
        return;
    }

    // net.c içinde: ARP resolve + ICMP echo
    int ok = net_ping_ipv4(ip_be);

    if (!ok) {
        commands_puts("Ping basarisiz / timeout.\n");
    }
}

REGISTER_COMMAND(ping, cmd_ping, "ICMP ping atar (ornek: ping 10.0.2.2)");