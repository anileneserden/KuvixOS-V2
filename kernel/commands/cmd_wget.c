// kernel/commands/cmd_wget.c
#include <kernel/printk.h>
#include <kernel/drivers/net/net.h>
#include <lib/commands.h>
#include <lib/string.h>
#include <stdint.h>

static int parse_u16(const char* s, uint16_t* out) {
    if (!s || !out) return 0;
    uint32_t v = 0;
    for (int i=0; s[i]; i++) {
        if (s[i] < '0' || s[i] > '9') return 0;
        v = v * 10 + (uint32_t)(s[i] - '0');
        if (v > 65535) return 0;
    }
    *out = (uint16_t)v;
    return 1;
}

// "a.b.c.d" -> BE u32
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

static void print_ip_be(uint32_t ip_be) {
    printk("%u.%u.%u.%u",
        (unsigned)((ip_be >> 24) & 0xFF),
        (unsigned)((ip_be >> 16) & 0xFF),
        (unsigned)((ip_be >>  8) & 0xFF),
        (unsigned)( ip_be        & 0xFF));
}

// req buffer'a string append (taşarsa 0 döner)
static int buf_append(char* buf, int cap, int* io_len, const char* s) {
    if (!buf || !io_len || !s) return 0;
    int n = *io_len;
    for (int i=0; s[i]; i++) {
        if (n >= cap - 1) {
            buf[cap - 1] = 0;
            return 0;
        }
        buf[n++] = s[i];
    }
    buf[n] = 0;
    *io_len = n;
    return 1;
}

static int buf_append_path_line(char* buf, int cap, int* io_len, const char* path) {
    if (!buf_append(buf, cap, io_len, "GET ")) return 0;
    if (!buf_append(buf, cap, io_len, path)) return 0;
    if (!buf_append(buf, cap, io_len, " HTTP/1.0\r\n")) return 0;
    return 1;
}

static int build_http_request(char* req, int req_cap, const char* host, const char* path, int* out_len) {
    int n = 0;
    req[0] = 0;

    if (!buf_append_path_line(req, req_cap, &n, path)) return 0;

    if (!buf_append(req, req_cap, &n, "Host: ")) return 0;
    if (!buf_append(req, req_cap, &n, host)) return 0;
    if (!buf_append(req, req_cap, &n, "\r\n")) return 0;

    if (!buf_append(req, req_cap, &n, "Connection: close\r\n")) return 0;
    if (!buf_append(req, req_cap, &n, "\r\n")) return 0;

    if (out_len) *out_len = n;
    return 1;
}

static void cmd_wget(int argc, char** argv) {
    if (argc < 4) {
        commands_puts("Kullanim: wget <host|ip> <port> </path>\n");
        commands_puts("Ornek   : wget 10.0.2.2 8080 /test.txt\n");
        commands_puts("Ornek   : wget example.com 80 /\n");
        return;
    }

    const char* host = argv[1];
    const char* port_str = argv[2];
    const char* path = argv[3];

    if (!path || path[0] != '/') {
        commands_puts("Path '/' ile baslamali. Ornek: /test.txt\n");
        return;
    }

    uint16_t port = 0;
    if (!parse_u16(port_str, &port)) {
        commands_puts("Port gecersiz.\n");
        return;
    }

    uint32_t ip_be = 0;

    // Önce IP mi diye bak
    if (!parse_ip4_be(host, &ip_be)) {
        // IP değilse DNS çöz
        uint32_t dns_ip_be = 0x0A000203; // 10.0.2.3 (QEMU slirp DNS)
        printk("[WGET] DNS resolving: %s via ", host);
        print_ip_be(dns_ip_be);
        printk("\n");

        if (!net_dns_resolve_a(host, dns_ip_be, &ip_be)) {
            commands_puts("DNS resolve basarisiz.\n");
            return;
        }

        printk("[WGET] %s -> ", host);
        print_ip_be(ip_be);
        printk("\n");
    }

    if (!net_tcp_connect(ip_be, port)) {
        commands_puts("TCP connect basarisiz.\n");
        return;
    }

    // HTTP request build
    char req[512];
    int req_len = 0;
    if (!build_http_request(req, (int)sizeof(req), host, path, &req_len)) {
        commands_puts("Request buffer tasdi (host/path cok uzun?).\n");
        net_tcp_close();
        return;
    }

    if (!net_tcp_send((const uint8_t*)req, (uint16_t)req_len)) {
        commands_puts("TCP send basarisiz.\n");
        net_tcp_close();
        return;
    }

    // Response oku: header bitene kadar \r\n\r\n ara, sonra body bas
    uint8_t buf[1024];
    int header_done = 0;
    int printed_any = 0;

    // header split olabileceği için küçük state
    uint8_t last4[4] = {0,0,0,0};

    while (1) {
        int r = net_tcp_recv(buf, (uint16_t)sizeof(buf), 12000000);
        if (r <= 0) break;

        int i = 0;

        if (!header_done) {
            for (; i < r; i++) {
                last4[0] = last4[1];
                last4[1] = last4[2];
                last4[2] = last4[3];
                last4[3] = buf[i];

                if (last4[0] == '\r' && last4[1] == '\n' &&
                    last4[2] == '\r' && last4[3] == '\n') {
                    header_done = 1;
                    i++; // body başlangıcı
                    break;
                }
            }
        }

        if (header_done) {
            for (; i < r; i++) {
                printk("%c", (char)buf[i]);
            }
            printed_any = 1;
        }
    }

    if (printed_any) printk("\n");
    net_tcp_close();
}

REGISTER_COMMAND(wget, cmd_wget, "HTTP ile dosya ceker (wget <host|ip> <port> </path>)");