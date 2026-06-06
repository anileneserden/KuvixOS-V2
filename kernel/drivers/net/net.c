// kernel/drivers/net/net.c
#include <kernel/drivers/net/net.h>
#include <kernel/drivers/net/e1000.h>
#include <kernel/drivers/pci/pci.h>
#include <kernel/printk.h>
#include <lib/string.h>
#include <stdint.h>

// ---------------- helpers ----------------
static inline uint16_t rd16be(const uint8_t* p) { return ((uint16_t)p[0] << 8) | p[1]; }
static inline uint32_t rd32be(const uint8_t* p) { return ((uint32_t)p[0]<<24)|((uint32_t)p[1]<<16)|((uint32_t)p[2]<<8)|p[3]; }
static inline void wr16be(uint8_t* p, uint16_t v){ p[0]=(uint8_t)(v>>8); p[1]=(uint8_t)v; }
static inline void wr32be(uint8_t* p, uint32_t v){ p[0]=(uint8_t)(v>>24); p[1]=(uint8_t)(v>>16); p[2]=(uint8_t)(v>>8); p[3]=(uint8_t)v; }

static void net_poll_once(void);

static uint16_t csum16(const void* data, uint32_t len) {
    const uint8_t* p = (const uint8_t*)data;
    uint32_t sum = 0;
    while (len > 1) { sum += ((uint16_t)p[0] << 8) | p[1]; p += 2; len -= 2; }
    if (len) sum += ((uint16_t)p[0] << 8);
    while (sum >> 16) sum = (sum & 0xFFFF) + (sum >> 16);
    return (uint16_t)(~sum);
}

// ---------------- IPv4 config (şimdilik statik) ----------------
static uint32_t g_ip   = 0x0A00020F;   // 10.0.2.15
static uint32_t g_mask = 0xFFFFFF00;   // 255.255.255.0
static uint32_t g_gw   = 0x0A000202;   // 10.0.2.2

void net_set_ipv4(uint32_t ip_be, uint32_t mask_be, uint32_t gw_be) {
    g_ip = ip_be; g_mask = mask_be; g_gw = gw_be;
}

static int same_subnet(uint32_t a, uint32_t b) {
    return ((a & g_mask) == (b & g_mask));
}

// ---------------- ARP cache ----------------
typedef struct {
    uint32_t ip;
    uint8_t  mac[6];
    int      valid;
} arp_entry_t;

static arp_entry_t g_arp[8];

static void arp_put(uint32_t ip, const uint8_t mac[6]) {
    for (int i=0;i<8;i++) {
        if (g_arp[i].valid && g_arp[i].ip == ip) { memcpy(g_arp[i].mac, mac, 6); return; }
    }
    for (int i=0;i<8;i++) {
        if (!g_arp[i].valid) { g_arp[i].valid=1; g_arp[i].ip=ip; memcpy(g_arp[i].mac, mac, 6); return; }
    }
    g_arp[0].valid=1; g_arp[0].ip=ip; memcpy(g_arp[0].mac, mac, 6);
}

static int arp_get(uint32_t ip, uint8_t out_mac[6]) {
    for (int i=0;i<8;i++) {
        if (g_arp[i].valid && g_arp[i].ip == ip) { memcpy(out_mac, g_arp[i].mac, 6); return 1; }
    }
    return 0;
}

static void send_arp_who_has(uint32_t target_ip) {
    uint8_t payload[28];

    wr16be(&payload[0], 1);        // htype Ethernet
    wr16be(&payload[2], 0x0800);   // ptype IPv4
    payload[4]=6; payload[5]=4;    // hlen/plen
    wr16be(&payload[6], 1);        // oper request

    uint8_t smac[6]; e1000_get_mac(smac);
    memcpy(&payload[8], smac, 6);
    wr32be(&payload[14], g_ip);

    memset(&payload[18], 0, 6);
    wr32be(&payload[24], target_ip);

    uint8_t bcast[6]={0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};
    e1000_send_eth(bcast, 0x0806, payload, sizeof(payload));
}

// ---------------- ICMP ping ----------------
static volatile int g_ping_got = 0;
static uint16_t g_ping_id  = 0x1234;
static uint16_t g_ping_seq = 1;
static uint32_t g_ping_dst = 0;

static void handle_arp(const uint8_t* frame, uint16_t len) {
    if (len < 14+28) return;
    const uint8_t* a = frame + 14;
    uint16_t oper = rd16be(a+6);
    if (oper != 2) return; // reply
    const uint8_t* sha = a+8;
    uint32_t spa = rd32be(a+14);
    arp_put(spa, sha);
}

static void handle_ipv4_icmp(const uint8_t* frame, uint16_t len) {
    if (len < 14+20) return;
    const uint8_t* ip = frame + 14;
    uint8_t ihl = (ip[0] & 0x0F) * 4;
    if (ihl < 20) return;
    if (len < 14 + ihl + 8) return;

    if (ip[9] != 1) return; // ICMP

    uint32_t src = rd32be(ip+12);
    uint32_t dst = rd32be(ip+16);
    if (dst != g_ip) return;

    const uint8_t* icmp = ip + ihl;
    uint8_t type = icmp[0];
    if (type != 0) return; // echo reply

    uint16_t id  = rd16be(icmp+4);
    uint16_t seq = rd16be(icmp+6);

    if (src == g_ping_dst && id == g_ping_id && seq == g_ping_seq) {
        g_ping_got = 1;
    }
}

static void send_icmp_echo(uint32_t dst_ip, const uint8_t dst_mac[6]) {
    uint8_t pkt[20 + 8 + 16];
    memset(pkt, 0, sizeof(pkt));

    // IPv4 header
    pkt[0] = 0x45;
    pkt[8] = 64;
    pkt[9] = 1; // ICMP
    wr16be(&pkt[2], (uint16_t)sizeof(pkt));
    wr32be(&pkt[12], g_ip);
    wr32be(&pkt[16], dst_ip);
    wr16be(&pkt[10], csum16(pkt, 20));

    // ICMP echo request
    uint8_t* ic = &pkt[20];
    ic[0] = 8;
    ic[1] = 0;
    wr16be(&ic[4], g_ping_id);
    wr16be(&ic[6], g_ping_seq);
    for (int i=0;i<16;i++) ic[8+i] = (uint8_t)('A' + i);
    wr16be(&ic[2], csum16(ic, 8+16));

    e1000_send_eth(dst_mac, 0x0800, pkt, sizeof(pkt));
}

// ---------------- TCP minimal ----------------
#define TCP_FLAG_FIN 0x01
#define TCP_FLAG_SYN 0x02
#define TCP_FLAG_RST 0x04
#define TCP_FLAG_PSH 0x08
#define TCP_FLAG_ACK 0x10

typedef enum {
    TCP_CLOSED = 0,
    TCP_SYN_SENT,
    TCP_ESTABLISHED,
    TCP_FIN_WAIT,
} tcp_state_t;

static struct {
    tcp_state_t st;
    uint32_t dst_ip;
    uint16_t dst_port;
    uint16_t src_port;
    uint32_t seq;
    uint32_t ack;

    volatile int connected;
    volatile int closed;

    uint8_t  rxbuf[2048];
    volatile uint16_t rxlen;
    volatile int rxready;
} g_tcp;

static uint16_t tcp_checksum(uint32_t src_ip, uint32_t dst_ip, const uint8_t* tcp, uint16_t tcp_len) {
    uint8_t pseudo[12];
    wr32be(&pseudo[0], src_ip);
    wr32be(&pseudo[4], dst_ip);
    pseudo[8] = 0;
    pseudo[9] = 6; // TCP
    wr16be(&pseudo[10], tcp_len);

    static uint8_t buf[2048];
    uint32_t total = 12 + tcp_len;
    if (total > sizeof(buf)) return 0;
    memcpy(buf, pseudo, 12);
    memcpy(buf + 12, tcp, tcp_len);
    return csum16(buf, total);
}

static void tcp_send_segment(const uint8_t dst_mac[6], uint32_t dst_ip,
                             uint16_t src_port, uint16_t dst_port,
                             uint32_t seq, uint32_t ack,
                             uint8_t flags,
                             const uint8_t* payload, uint16_t payload_len)
{
    if (payload_len > 1460) payload_len = 1460;

    uint16_t ip_len = 20 + 20 + payload_len;
    uint8_t pkt[20 + 20 + 1460];
    memset(pkt, 0, ip_len);

    // IPv4
    pkt[0] = 0x45;
    pkt[8] = 64;
    pkt[9] = 6; // TCP
    wr16be(&pkt[2], ip_len);
    wr32be(&pkt[12], g_ip);
    wr32be(&pkt[16], dst_ip);
    wr16be(&pkt[10], csum16(pkt, 20));

    // TCP
    uint8_t* t = &pkt[20];
    wr16be(&t[0], src_port);
    wr16be(&t[2], dst_port);
    wr32be(&t[4], seq);
    wr32be(&t[8], ack);
    t[12] = (5 << 4);
    t[13] = flags;
    wr16be(&t[14], 4096);
    wr16be(&t[16], 0); // checksum later

    if (payload && payload_len) memcpy(&t[20], payload, payload_len);

    uint16_t tcp_len = 20 + payload_len;
    wr16be(&t[16], tcp_checksum(g_ip, dst_ip, t, tcp_len));

    e1000_send_eth(dst_mac, 0x0800, pkt, ip_len);
}

static void tcp_handle_ipv4_tcp(const uint8_t* frame, uint16_t len) {
    if (len < 14 + 20) return;

    const uint8_t* ip = frame + 14;
    uint8_t ihl = (ip[0] & 0x0F) * 4;
    if (ihl < 20) return;
    if (len < 14 + ihl + 20) return;
    if (ip[9] != 6) return; // TCP

    uint32_t src_ip = rd32be(ip + 12);
    uint32_t dst_ip = rd32be(ip + 16);
    if (dst_ip != g_ip) return;

    const uint8_t* t = ip + ihl;
    uint16_t src_port = rd16be(t + 0);
    uint16_t dst_port = rd16be(t + 2);
    uint32_t seq = rd32be(t + 4);
    uint8_t off = (t[12] >> 4) * 4;
    uint8_t flags = t[13];

    if (dst_port != g_tcp.src_port) return;
    if (src_ip != g_tcp.dst_ip) return;
    if (src_port != g_tcp.dst_port) return;

    uint16_t ip_total = rd16be(ip + 2);
    uint16_t tcp_total = (ip_total >= ihl) ? (ip_total - ihl) : 0;
    if (tcp_total < off) return;

    uint16_t data_len = (tcp_total > off) ? (tcp_total - off) : 0;
    const uint8_t* data = t + off;

    uint32_t next_hop = same_subnet(g_ip, g_tcp.dst_ip) ? g_tcp.dst_ip : g_gw;

    // SYN+ACK
    if (g_tcp.st == TCP_SYN_SENT &&
        (flags & (TCP_FLAG_SYN | TCP_FLAG_ACK)) == (TCP_FLAG_SYN | TCP_FLAG_ACK)) {

        g_tcp.ack = seq + 1;

        uint8_t mac[6];
        if (!arp_get(next_hop, mac)) return;

        tcp_send_segment(mac, g_tcp.dst_ip, g_tcp.src_port, g_tcp.dst_port,
                         g_tcp.seq, g_tcp.ack, TCP_FLAG_ACK, 0, 0);

        g_tcp.st = TCP_ESTABLISHED;
        g_tcp.connected = 1;
        return;
    }

    if (g_tcp.st == TCP_ESTABLISHED) {
        // data
        if (data_len > 0 && !g_tcp.rxready) {
            if (data_len > sizeof(g_tcp.rxbuf)) data_len = sizeof(g_tcp.rxbuf);

            memcpy(g_tcp.rxbuf, data, data_len);
            g_tcp.rxlen = data_len;
            g_tcp.rxready = 1;

            g_tcp.ack = seq + data_len;

            uint8_t mac[6];
            if (arp_get(next_hop, mac)) {
                tcp_send_segment(mac, g_tcp.dst_ip, g_tcp.src_port, g_tcp.dst_port,
                                 g_tcp.seq, g_tcp.ack, TCP_FLAG_ACK, 0, 0);
            }
        }

        if (flags & TCP_FLAG_FIN) {
            g_tcp.ack = seq + 1;

            uint8_t mac[6];
            if (arp_get(next_hop, mac)) {
                tcp_send_segment(mac, g_tcp.dst_ip, g_tcp.src_port, g_tcp.dst_port,
                                 g_tcp.seq, g_tcp.ack, TCP_FLAG_ACK, 0, 0);
            }

            g_tcp.closed = 1;
        }
    }
}

// ---------------- UDP minimal ----------------
static struct {
    volatile int ready;
    uint16_t listen_port;
    uint32_t src_ip;
    uint16_t src_port;
    uint8_t  buf[512];
    uint16_t len;
} g_udp;

static uint16_t udp_checksum(uint32_t src_ip, uint32_t dst_ip, const uint8_t* udp, uint16_t udp_len) {
    uint8_t pseudo[12];
    wr32be(&pseudo[0], src_ip);
    wr32be(&pseudo[4], dst_ip);
    pseudo[8] = 0;
    pseudo[9] = 17; // UDP
    wr16be(&pseudo[10], udp_len);

    static uint8_t buf[1024];
    uint32_t total = 12 + udp_len;
    if (total > sizeof(buf)) return 0;

    memcpy(buf, pseudo, 12);
    memcpy(buf + 12, udp, udp_len);

    return csum16(buf, total);
}

int net_udp_send(uint32_t dst_ip_be, uint16_t src_port, uint16_t dst_port,
                 const uint8_t* data, uint16_t len)
{
    if (!e1000_is_ready()) return 0;
    if (len > 1472) len = 1472;

    uint32_t next_hop = same_subnet(g_ip, dst_ip_be) ? dst_ip_be : g_gw;

    uint8_t mac[6];
    if (!arp_get(next_hop, mac)) {
        send_arp_who_has(next_hop);
        for (int i=0;i<8000000;i++) net_poll_once();
        if (!arp_get(next_hop, mac)) {
            printk("[UDP] ARP resolve failed\n");
            return 0;
        }
    }

    uint16_t ip_len = 20 + 8 + len;
    uint8_t pkt[20 + 8 + 1472];
    memset(pkt, 0, ip_len);

    // IPv4
    pkt[0] = 0x45;
    pkt[8] = 64;
    pkt[9] = 17; // UDP
    wr16be(&pkt[2], ip_len);
    wr32be(&pkt[12], g_ip);
    wr32be(&pkt[16], dst_ip_be);
    wr16be(&pkt[10], csum16(pkt, 20));

    // UDP
    uint8_t* u = &pkt[20];
    wr16be(&u[0], src_port);
    wr16be(&u[2], dst_port);
    wr16be(&u[4], (uint16_t)(8 + len));
    wr16be(&u[6], 0);

    if (data && len) memcpy(&u[8], data, len);

    // İstersen geçici olarak 0 bırakabilirsin; ama checksum hesaplıyoruz
    {
        uint16_t c = udp_checksum(g_ip, dst_ip_be, u, (uint16_t)(8 + len));
        if (c == 0) c = 0xFFFF;
        wr16be(&u[6], c);
    }

    return e1000_send_eth(mac, 0x0800, pkt, ip_len);
}

static void udp_handle_ipv4_udp(const uint8_t* frame, uint16_t len) {
    if (len < 14 + 20 + 8) return;

    const uint8_t* ip = frame + 14;
    uint8_t ihl = (ip[0] & 0x0F) * 4;
    if (ihl < 20) return;
    if (len < 14 + ihl + 8) return;
    if (ip[9] != 17) return; // UDP

    uint32_t src_ip = rd32be(ip + 12);
    uint32_t dst_ip = rd32be(ip + 16);
    if (dst_ip != g_ip) return;

    const uint8_t* u = ip + ihl;
    uint16_t src_port = rd16be(u + 0);
    uint16_t dst_port = rd16be(u + 2);
    uint16_t udp_len  = rd16be(u + 4);

    if (udp_len < 8) return;
    if (14 + ihl + udp_len > len) return;
    if (dst_port != g_udp.listen_port) return;
    if (g_udp.ready) return;

    uint16_t data_len = (uint16_t)(udp_len - 8);
    if (data_len > sizeof(g_udp.buf)) data_len = sizeof(g_udp.buf);

    memcpy(g_udp.buf, u + 8, data_len);
    g_udp.len = data_len;
    g_udp.src_ip = src_ip;
    g_udp.src_port = src_port;
    g_udp.ready = 1;
}

int net_udp_recv(uint16_t listen_port,
                 uint32_t* out_src_ip_be, uint16_t* out_src_port,
                 uint8_t* out, uint16_t out_max,
                 uint32_t spin_timeout)
{
    g_udp.listen_port = listen_port;
    g_udp.ready = 0;
    g_udp.len = 0;
    g_udp.src_ip = 0;
    g_udp.src_port = 0;

    for (uint32_t i=0; i<spin_timeout; i++) {
        net_poll_once();
        if (g_udp.ready) {
            uint16_t n = g_udp.len;
            if (n > out_max) n = out_max;
            memcpy(out, g_udp.buf, n);
            if (out_src_ip_be) *out_src_ip_be = g_udp.src_ip;
            if (out_src_port)  *out_src_port  = g_udp.src_port;
            g_udp.ready = 0;
            g_udp.len = 0;
            return (int)n;
        }
    }
    return 0;
}

// ---------------- DNS ----------------
static uint16_t g_dns_txid = 0x4000;

static int dns_write_qname(uint8_t* out, int out_cap, const char* host) {
    int n = 0;
    int label_len = 0;
    int label_start = 0;

    if (!out || !host || out_cap <= 0) return -1;

    for (int i=0;; i++) {
        char c = host[i];
        if (c == '.' || c == 0) {
            int len = i - label_start;
            if (len <= 0 || len > 63) return -1;
            if (n + 1 + len >= out_cap) return -1;
            out[n++] = (uint8_t)len;
            for (int j=0; j<len; j++) out[n++] = (uint8_t)host[label_start + j];
            label_start = i + 1;
            label_len = 0;
            if (c == 0) break;
            continue;
        }
        label_len++;
        if (label_len > 63) return -1;
    }

    if (n + 1 >= out_cap) return -1;
    out[n++] = 0;
    return n;
}

static int dns_skip_name(const uint8_t* msg, int msg_len, int off) {
    if (off < 0 || off >= msg_len) return -1;

    while (off < msg_len) {
        uint8_t c = msg[off];
        if (c == 0) return off + 1;

        // compression pointer
        if ((c & 0xC0) == 0xC0) {
            if (off + 1 >= msg_len) return -1;
            return off + 2;
        }

        off++;
        if (off + c > msg_len) return -1;
        off += c;
    }

    return -1;
}

int net_dns_resolve_a(const char* host, uint32_t dns_ip_be, uint32_t* out_ip_be) {
    if (!host || !out_ip_be) return 0;

    uint8_t req[512];
    memset(req, 0, sizeof(req));

    uint16_t txid = ++g_dns_txid;
    if (txid == 0) txid = ++g_dns_txid;

    // DNS header
    wr16be(&req[0], txid);
    wr16be(&req[2], 0x0100); // standard query, RD=1
    wr16be(&req[4], 1);      // QDCOUNT
    wr16be(&req[6], 0);      // ANCOUNT
    wr16be(&req[8], 0);      // NSCOUNT
    wr16be(&req[10], 0);     // ARCOUNT

    int n = 12;
    int qn = dns_write_qname(req + n, (int)sizeof(req) - n, host);
    if (qn < 0) return 0;
    n += qn;

    if (n + 4 > (int)sizeof(req)) return 0;
    wr16be(&req[n], 1); n += 2; // QTYPE=A
    wr16be(&req[n], 1); n += 2; // QCLASS=IN

    if (!net_udp_send(dns_ip_be, 53000, 53, req, (uint16_t)n)) {
        printk("[DNS] send failed\n");
        return 0;
    }

    uint8_t resp[512];
    uint32_t src_ip = 0;
    uint16_t src_port = 0;
    int r = net_udp_recv(53000, &src_ip, &src_port, resp, sizeof(resp), 30000000);
    if (r <= 0) {
        printk("[DNS] timeout\n");
        return 0;
    }

    if (src_ip != dns_ip_be || src_port != 53) {
        printk("[DNS] unexpected source\n");
        return 0;
    }

    if (r < 12) return 0;
    if (rd16be(&resp[0]) != txid) return 0;

    uint16_t flags   = rd16be(&resp[2]);
    uint16_t qdcount = rd16be(&resp[4]);
    uint16_t ancount = rd16be(&resp[6]);

    if ((flags & 0x8000) == 0) return 0;      // response
    if ((flags & 0x000F) != 0) return 0;      // RCODE == 0
    if (qdcount < 1 || ancount < 1) return 0;

    int off = 12;

    // questions skip
    for (uint16_t i=0; i<qdcount; i++) {
        off = dns_skip_name(resp, r, off);
        if (off < 0 || off + 4 > r) return 0;
        off += 4; // qtype + qclass
    }

    // answers scan
    for (uint16_t i=0; i<ancount; i++) {
        off = dns_skip_name(resp, r, off);
        if (off < 0 || off + 10 > r) return 0;

        uint16_t type   = rd16be(&resp[off]); off += 2;
        uint16_t klass  = rd16be(&resp[off]); off += 2;
        uint32_t ttl    = rd32be(&resp[off]); (void)ttl; off += 4;
        uint16_t rdlen  = rd16be(&resp[off]); off += 2;

        if (off + rdlen > r) return 0;

        if (type == 1 && klass == 1 && rdlen == 4) {
            *out_ip_be = rd32be(&resp[off]);
            return 1;
        }

        off += rdlen;
    }

    return 0;
}

// ---------------- RX poll ----------------
static void net_poll_once(void) {
    uint8_t frame[1600];
    uint16_t len = 0;
    if (!e1000_rx_pop(frame, sizeof(frame), &len)) return;
    if (len < 14) return;

    uint16_t eth = rd16be(&frame[12]);
    if (eth == 0x0806) {
        handle_arp(frame, len);
    } else if (eth == 0x0800) {
        handle_ipv4_icmp(frame, len);
        tcp_handle_ipv4_tcp(frame, len);
        udp_handle_ipv4_udp(frame, len);
    }
}

// ---------------- Public API ----------------
void net_init(void) {
    printk("[NET] net_init()\n");
    pci_scan_dump_nics(); // e1000_probe tetikler
}

int net_ping_ipv4(uint32_t dst_ip_be) {
    if (!e1000_is_ready()) {
        printk("[PING] e1000 not ready\n");
        return 0;
    }

    uint32_t next_hop = same_subnet(g_ip, dst_ip_be) ? dst_ip_be : g_gw;

    uint8_t mac[6];
    if (!arp_get(next_hop, mac)) {
        send_arp_who_has(next_hop);
        for (int i=0;i<8000000;i++) net_poll_once();
        if (!arp_get(next_hop, mac)) {
            printk("[PING] ARP resolve failed\n");
            return 0;
        }
    }

    g_ping_dst = dst_ip_be;
    g_ping_got = 0;
    send_icmp_echo(dst_ip_be, mac);

    for (int i=0;i<12000000;i++) {
        net_poll_once();
        if (g_ping_got) {
            printk("[PING] reply from %u.%u.%u.%u (id=%u seq=%u)\n",
                (dst_ip_be>>24)&0xFF,(dst_ip_be>>16)&0xFF,(dst_ip_be>>8)&0xFF,dst_ip_be&0xFF,
                (unsigned)g_ping_id,(unsigned)g_ping_seq);
            return 1;
        }
    }

    printk("[PING] timeout\n");
    return 0;
}

int net_tcp_connect(uint32_t dst_ip_be, uint16_t dst_port) {
    if (!e1000_is_ready()) {
        printk("[TCP] e1000 not ready\n");
        return 0;
    }

    memset(&g_tcp, 0, sizeof(g_tcp));
    g_tcp.dst_ip = dst_ip_be;
    g_tcp.dst_port = dst_port;
    g_tcp.src_port = 40000;
    g_tcp.seq = 0x1000;
    g_tcp.ack = 0;
    g_tcp.st = TCP_SYN_SENT;

    uint32_t next_hop = same_subnet(g_ip, dst_ip_be) ? dst_ip_be : g_gw;

    uint8_t mac[6];
    if (!arp_get(next_hop, mac)) {
        send_arp_who_has(next_hop);
        for (int i=0;i<8000000;i++) net_poll_once();
        if (!arp_get(next_hop, mac)) {
            printk("[TCP] ARP resolve failed\n");
            return 0;
        }
    }

    tcp_send_segment(mac, dst_ip_be, g_tcp.src_port, g_tcp.dst_port,
                     g_tcp.seq, 0, TCP_FLAG_SYN, 0, 0);
    g_tcp.seq += 1;

    for (int i=0;i<12000000;i++) {
        net_poll_once();
        if (g_tcp.connected) return 1;
    }

    printk("[TCP] connect timeout\n");
    return 0;
}

int net_tcp_send(const uint8_t* data, uint16_t len) {
    if (g_tcp.st != TCP_ESTABLISHED) return 0;

    uint32_t next_hop = same_subnet(g_ip, g_tcp.dst_ip) ? g_tcp.dst_ip : g_gw;
    uint8_t mac[6];
    if (!arp_get(next_hop, mac)) return 0;

    tcp_send_segment(mac, g_tcp.dst_ip, g_tcp.src_port, g_tcp.dst_port,
                     g_tcp.seq, g_tcp.ack, TCP_FLAG_ACK | TCP_FLAG_PSH,
                     data, len);
    g_tcp.seq += len;
    return 1;
}

int net_tcp_recv(uint8_t* out, uint16_t maxlen, uint32_t spin_timeout) {
    for (uint32_t i=0;i<spin_timeout;i++) {
        net_poll_once();
        if (g_tcp.rxready) {
            uint16_t n = g_tcp.rxlen;
            if (n > maxlen) n = maxlen;
            memcpy(out, g_tcp.rxbuf, n);
            g_tcp.rxready = 0;
            g_tcp.rxlen = 0;
            return (int)n;
        }
        if (g_tcp.closed) return 0;
    }
    return 0;
}

int net_tcp_close(void) {
    if (g_tcp.st != TCP_ESTABLISHED) return 1;

    uint32_t next_hop = same_subnet(g_ip, g_tcp.dst_ip) ? g_tcp.dst_ip : g_gw;
    uint8_t mac[6];
    if (!arp_get(next_hop, mac)) return 0;

    tcp_send_segment(mac, g_tcp.dst_ip, g_tcp.src_port, g_tcp.dst_port,
                     g_tcp.seq, g_tcp.ack, TCP_FLAG_FIN | TCP_FLAG_ACK, 0, 0);
    g_tcp.seq += 1;
    g_tcp.st = TCP_FIN_WAIT;

    for (int i=0;i<8000000;i++) {
        net_poll_once();
        if (g_tcp.closed) return 1;
    }
    return 1;
}

void net_get_ipv4(uint32_t* ip_be, uint32_t* mask_be, uint32_t* gw_be) {
    if (ip_be)   *ip_be   = g_ip;
    if (mask_be) *mask_be = g_mask;
    if (gw_be)   *gw_be   = g_gw;
}

// Basit stub; istersen sonra gerçek implement ederiz
int net_http_get_to_buf(uint32_t ip_be, uint16_t port, const char* path,
                        char* out, int out_cap, int* out_len)
{
    (void)ip_be; (void)port; (void)path; (void)out; (void)out_cap;
    if (out_len) *out_len = 0;
    return 0;
}