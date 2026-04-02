// kernel/drivers/net/net.c
#include <kernel/drivers/net/net.h>
#include <kernel/drivers/net/e1000.h>
#include <kernel/drivers/net/pci.h>
#include <kernel/printk.h>
#include <lib/string.h>
#include <stdint.h>

// ---------------- Byte Order & Helpers ----------------
static inline uint16_t rd16be(const uint8_t* p) { return ((uint16_t)p[0] << 8) | p[1]; }
static inline uint32_t rd32be(const uint8_t* p) { return ((uint32_t)p[0]<<24)|((uint32_t)p[1]<<16)|((uint32_t)p[2]<<8)|p[3]; }
static inline void wr16be(uint8_t* p, uint16_t v){ p[0]=(uint8_t)(v>>8); p[1]=(uint8_t)v; }
static inline void wr32be(uint8_t* p, uint32_t v){ p[0]=(uint8_t)(v>>24); p[1]=(uint8_t)(v>>16); p[2]=(uint8_t)(v>>8); p[3]=(uint8_t)v; }

static inline uint16_t wr16be_val(uint16_t v) { return ((v << 8) & 0xFF00) | ((v >> 8) & 0x00FF); }
static inline uint16_t rd16be_val(uint16_t v) { return wr16be_val(v); }

static void net_poll_once(void);

static uint16_t csum16(const void* data, uint32_t len) {
    const uint8_t* p = (const uint8_t*)data;
    uint32_t sum = 0;
    while (len > 1) { sum += ((uint16_t)p[0] << 8) | p[1]; p += 2; len -= 2; }
    if (len) sum += ((uint16_t)p[0] << 8);
    while (sum >> 16) sum = (sum & 0xFFFF) + (sum >> 16);
    return (uint16_t)(~sum);
}

// ---------------- IPv4 Config ----------------
static uint32_t g_ip   = 0x0A00020F;   // 10.0.2.15
static uint32_t g_mask = 0xFFFFFF00;   // 255.255.255.0
static uint32_t g_gw   = 0x0A000202;   // 10.0.2.2

void net_set_ipv4(uint32_t ip_be, uint32_t mask_be, uint32_t gw_be) {
    g_ip = ip_be; g_mask = mask_be; g_gw = gw_be;
}

void net_get_ipv4(uint32_t* ip_be, uint32_t* mask_be, uint32_t* gw_be) {
    if (ip_be)   *ip_be = g_ip;
    if (mask_be) *mask_be = g_mask;
    if (gw_be)   *gw_be = g_gw;
}

static int same_subnet(uint32_t a, uint32_t b) {
    return ((a & g_mask) == (b & g_mask));
}

// ---------------- ARP Cache ----------------
typedef struct { uint32_t ip; uint8_t mac[6]; int valid; } arp_entry_t;
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
    wr16be(&payload[0], 1); wr16be(&payload[2], 0x0800);
    payload[4]=6; payload[5]=4; wr16be(&payload[6], 1);
    uint8_t smac[6]; e1000_get_mac(smac);
    memcpy(&payload[8], smac, 6); wr32be(&payload[14], g_ip);
    memset(&payload[18], 0, 6); wr32be(&payload[24], target_ip);
    uint8_t bcast[6]={0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};
    e1000_send_eth(bcast, 0x0806, payload, sizeof(payload));
}

static void handle_arp(const uint8_t* frame, uint16_t len) {
    if (len < 14+28) return;
    const uint8_t* a = frame + 14;
    if (rd16be(a+6) == 2) arp_put(rd32be(a+14), a+8);
}

// ---------------- ICMP & UDP Globals ----------------
static volatile int g_ping_got = 0;
static uint32_t g_ping_dst = 0;
static struct { 
    volatile int ready; uint8_t buf[2048]; uint16_t len; 
    uint32_t ip; uint16_t port; 
} g_udp;

// ---------------- ICMP (Ping) ----------------
static void handle_ipv4_icmp(const uint8_t* frame, uint16_t len) {
    (void)len;
    const uint8_t* ip = frame + 14;
    uint8_t ihl = (ip[0] & 0x0F) * 4;
    if (ip[9] != 1) return;
    const uint8_t* icmp = ip + ihl;
    if (icmp[0] == 0 && rd32be(ip+12) == g_ping_dst) g_ping_got = 1;
}

int net_ping_ipv4(uint32_t dst_ip_be) {
    uint8_t mac[6];
    uint32_t next_hop = same_subnet(g_ip, dst_ip_be) ? dst_ip_be : g_gw;
    if (!arp_get(next_hop, mac)) {
        send_arp_who_has(next_hop);
        for (int i=0; i<5000000; i++) net_poll_once();
        if (!arp_get(next_hop, mac)) return 0;
    }
    g_ping_dst = dst_ip_be; g_ping_got = 0;
    uint8_t pkt[48]; memset(pkt, 0, 48);
    pkt[0]=0x45; pkt[8]=64; pkt[9]=1; wr16be(&pkt[2], 48);
    wr32be(&pkt[12], g_ip); wr32be(&pkt[16], dst_ip_be);
    wr16be(&pkt[10], csum16(pkt, 20));
    uint8_t* ic = pkt+20; ic[0]=8; wr16be(&ic[4], 0x1234); wr16be(&ic[6], 1);
    wr16be(&ic[2], csum16(ic, 28));
    e1000_send_eth(mac, 0x0800, pkt, 48);
    for (int i=0; i<10000000; i++) { net_poll_once(); if (g_ping_got) return 1; }
    return 0;
}

// ---------------- TCP Core ----------------
#define TCP_FLAG_FIN 0x01
#define TCP_FLAG_SYN 0x02
#define TCP_FLAG_PSH 0x08
#define TCP_FLAG_ACK 0x10

typedef enum { TCP_CLOSED, TCP_SYN_SENT, TCP_ESTABLISHED } tcp_state_t;
static struct {
    tcp_state_t st; uint32_t dst_ip; uint16_t dst_port; uint16_t src_port;
    uint32_t seq; uint32_t ack; volatile int connected, closed, rxready;
    uint8_t rxbuf[4096]; uint16_t rxlen;
} g_tcp;

static uint16_t g_next_src_port = 40000;

static uint16_t tcp_checksum(uint32_t src_ip, uint32_t dst_ip, uint8_t* tcp_seg, uint16_t tcp_len) {
    uint32_t sum = 0;

    // Pseudo Header (Sanal Başlık)
    sum += (src_ip >> 16) & 0xFFFF;
    sum += (src_ip & 0xFFFF);
    sum += (dst_ip >> 16) & 0xFFFF;
    sum += (dst_ip & 0xFFFF);
    sum += 0x0006; // Protocol TCP (6)
    sum += tcp_len;

    // TCP Header + Data
    uint16_t* ptr = (uint16_t*)tcp_seg;
    int count = tcp_len;
    while (count > 1) {
        // rd16be_val yerine doğrudan uint16_t okuyup byte swap yapıyoruz
        uint16_t val = *ptr++;
        sum += ((val << 8) & 0xFF00) | ((val >> 8) & 0x00FF);
        count -= 2;
    }
    if (count > 0) {
        sum += (uint16_t)(*(uint8_t*)ptr) << 8;
    }

    while (sum >> 16) sum = (sum & 0xFFFF) + (sum >> 16);
    return (uint16_t)(~sum);
}

static void tcp_send_seg(const uint8_t* dmac, uint32_t dip, uint16_t sp, uint16_t dp, uint32_t seq, uint32_t ack, uint8_t f, const uint8_t* p, uint16_t plen) {
    uint16_t ip_len = 40 + plen; // IP Header (20) + TCP Header (20) + Data
    uint8_t pkt[1600]; 
    memset(pkt, 0, 1600);

    // --- IPv4 Header ---
    pkt[0] = 0x45;          // Version (4) & IHL (5)
    pkt[8] = 64;            // TTL
    pkt[9] = 6;             // Protocol: TCP
    wr16be(&pkt[2], ip_len); 
    wr32be(&pkt[12], g_ip); 
    wr32be(&pkt[16], dip);
    wr16be(&pkt[10], csum16(pkt, 20)); // IP Checksum

    // --- TCP Header ---
    uint8_t* t = pkt + 20; 
    wr16be(t + 0, sp);       // Source Port
    wr16be(t + 2, dp);       // Dest Port
    wr32be(t + 4, seq);      // Sequence Number
    wr32be(t + 8, ack);      // ACK Number
    t[12] = 0x50;            // Header Length: 5 (20 bytes)
    t[13] = f;               // Flags
    wr16be(t + 14, 8192);    // Window Size

    // Payload (Veri) kopyalama
    if (p && plen) {
        memcpy(t + 20, p, plen);
    }

    // --- TCP Checksum (Kritik Bölge) ---
    // Önce checksum alanını sıfırla (fonksiyonun temiz hesap yapması için)
    t[16] = 0; 
    t[17] = 0;
    
    uint16_t cs = tcp_checksum(g_ip, dip, t, 20 + plen);
    
    // Checksum'ı doğrudan byte byte yazıyoruz (endianness hatasını önlemek için)
    t[16] = (uint8_t)(cs >> 8);
    t[17] = (uint8_t)(cs & 0xFF);

    e1000_send_eth(dmac, 0x0800, pkt, ip_len);
}

static void tcp_handle_ipv4_tcp(const uint8_t* frame, uint16_t len) {
    (void)len;
    const uint8_t *ip = frame + 14;
    uint8_t ihl = (ip[0] & 0x0F) * 4;
    const uint8_t *t = ip + ihl;
    uint16_t sp = rd16be(t), dp = rd16be(t + 2);
    uint32_t seq = rd32be(t + 4), ack_num = rd32be(t + 8);
    uint8_t off = (t[12] >> 4) * 4, f = t[13];

    if (sp == 8080 || dp == 40000) {
        printk("[TCP RX] Sp:%d Dp:%d Flags:0x%x Seq:%x Ack:%x\n", sp, dp, f, seq, ack_num);
    }

    if (dp != g_tcp.src_port || sp != g_tcp.dst_port) return;
    uint16_t dlen = rd16be(ip + 2) - ihl - off;
    uint8_t mac[6];
    if (!arp_get(same_subnet(g_ip, g_tcp.dst_ip) ? g_tcp.dst_ip : g_gw, mac)) return;

    if (g_tcp.st == TCP_SYN_SENT && (f & TCP_FLAG_SYN)) {
        g_tcp.ack = seq + 1; g_tcp.seq = ack_num;
        tcp_send_seg(mac, g_tcp.dst_ip, g_tcp.src_port, g_tcp.dst_port, g_tcp.seq, g_tcp.ack, TCP_FLAG_ACK, 0, 0);
        g_tcp.st = TCP_ESTABLISHED; g_tcp.connected = 1;
        printk("[TCP] Baglanti kuruldu!\n");
    } else if (g_tcp.st == TCP_ESTABLISHED) {
        uint16_t ip_total_len = rd16be(ip + 2);
        uint16_t tcp_header_len = off; // 'off' zaten (t[12] >> 4) * 4 olarak hesaplanmıştı
        int calculated_dlen = ip_total_len - ihl - tcp_header_len;

        if (calculated_dlen > 0) {
            printk("[TCP DATA] %d byte veri yakalandı!\n", calculated_dlen);

            int space = 4096 - (int)g_tcp.rxlen;
            int n = (calculated_dlen > space) ? space : calculated_dlen;
            if (n > 0) {
                memcpy(g_tcp.rxbuf + g_tcp.rxlen, t + off, n);
                g_tcp.rxlen += (uint16_t)n;
                g_tcp.rxready = 1;
            }

            g_tcp.ack = seq + calculated_dlen;
            tcp_send_seg(mac, g_tcp.dst_ip, g_tcp.src_port, g_tcp.dst_port, g_tcp.seq, g_tcp.ack, TCP_FLAG_ACK, 0, 0);
        }

        if (f & 0x01) { // FIN bayrağı
            g_tcp.closed = 1;
            g_tcp.ack = seq + 1;
            tcp_send_seg(mac, g_tcp.dst_ip, g_tcp.src_port, g_tcp.dst_port, g_tcp.seq, g_tcp.ack, TCP_FLAG_ACK, 0, 0);
        }
    }
}

// ---------------- DNS / UDP ----------------
int net_udp_send(uint32_t dip, uint16_t sp, uint16_t dp, const uint8_t* d, uint16_t len) {
    uint8_t mac[6]; uint32_t nh = same_subnet(g_ip, dip) ? dip : g_gw;
    if (!arp_get(nh, mac)) { send_arp_who_has(nh); for(int i=0;i<5000000;i++) net_poll_once(); if(!arp_get(nh, mac)) return 0; }
    uint16_t ilen = 28 + len; uint8_t pkt[1600]; memset(pkt, 0, 1600);
    pkt[0]=0x45; pkt[8]=64; pkt[9]=17; wr16be(pkt+2, ilen); wr32be(pkt+12, g_ip); wr32be(pkt+16, dip); wr16be(pkt+10, csum16(pkt, 20));
    uint8_t* u = pkt+20; wr16be(u, sp); wr16be(u+2, dp); wr16be(u+4, 8+len);
    if (d && len) memcpy(u+8, d, len);
    return e1000_send_eth(mac, 0x0800, pkt, ilen);
}

int net_udp_recv(uint16_t port, uint32_t* sip, uint16_t* sport, uint8_t* out, uint16_t max, uint32_t tout) {
    (void)port; g_udp.ready = 0;
    for (uint32_t i=0; i<tout; i++) {
        net_poll_once();
        if (g_udp.ready) {
            uint16_t n = (g_udp.len>max)?max:g_udp.len; memcpy(out, g_udp.buf, n);
            if (sip) *sip=g_udp.ip; if (sport) *sport=g_udp.port; return n;
        }
    }
    return 0;
}

int net_dns_resolve_a(const char* host, uint32_t dns_ip, uint32_t* out_ip) {
    (void)dns_ip; // Şimdilik statik döndüğümüz için susturuyoruz
    uint8_t req[512]; memset(req, 0, 12); wr16be(req, 0x4000); wr16be(req+2, 0x0100); wr16be(req+4, 1);
    int n = 12; 
    for (int i=0, s=0; ; i++) {
        if (host[i]=='.' || host[i]==0) { 
            req[n++]=(uint8_t)(i-s); 
            memcpy(req+n, host+s, i-s); 
            n+=(i-s); 
            s=i+1; 
            if (host[i]==0) break; 
        }
    }
    req[n++]=0; wr16be(req+n, 1); wr16be(req+n+2, 1); n+=4;
    // Şimdilik UDP trafiği ile uğraşmamak için doğrudan GW dönüyoruz
    *out_ip = 0x0A000202; 
    return 1; 
}

// ---------------- RX Poll ----------------
static void net_poll_once(void) {
    uint8_t frame[1600]; uint16_t len = 0;
    if (!e1000_rx_pop(frame, 1600, &len)) return;
    uint16_t eth = rd16be(frame+12);
    if (eth == 0x0806) handle_arp(frame, len);
    else if (eth == 0x0800) { handle_ipv4_icmp(frame, len); tcp_handle_ipv4_tcp(frame, len); }
}

// ---------------- Public API ----------------
void net_init(void) { pci_scan_dump_nics(); }

int net_tcp_connect(uint32_t dip, uint16_t dp) {
    uint16_t sport = g_next_src_port++;
    if (g_next_src_port > 60000) g_next_src_port = 40000;

    memset(&g_tcp, 0, sizeof(g_tcp));
    g_tcp.dst_ip = dip; g_tcp.dst_port = dp; g_tcp.src_port = sport; g_tcp.seq = 0x1000; g_tcp.st = TCP_SYN_SENT;
    uint8_t mac[6]; uint32_t nh = same_subnet(g_ip, dip) ? dip : g_gw;
    if (!arp_get(nh, mac)) {
        send_arp_who_has(nh); for(int i=0; i<10000000; i++) net_poll_once();
        if(!arp_get(nh, mac)) { printk("[TCP] ARP hata!\n"); return 0; }
    }
    printk("[TCP] SYN gonderiliyor... (port %d)\n", sport);
    tcp_send_seg(mac, dip, sport, dp, 0x1000, 0, TCP_FLAG_SYN, 0, 0);
    g_tcp.seq++;
    for (int i=0; i<200000000; i++) { net_poll_once(); if (g_tcp.connected) return 1; }
    return 0;
}

int net_tcp_send(const uint8_t* d, uint16_t l) {
    uint8_t m[6]; arp_get(same_subnet(g_ip, g_tcp.dst_ip)?g_tcp.dst_ip:g_gw, m);
    tcp_send_seg(m, g_tcp.dst_ip, g_tcp.src_port, g_tcp.dst_port, g_tcp.seq, g_tcp.ack, 0x18, d, l);
    g_tcp.seq+=l; return 1;
}

int net_tcp_recv(uint8_t* o, int m, uint32_t t) {
    for (uint32_t i = 0; i < t; i++) {
        // Kartı dırla
        net_poll_once(); 

        // Veri geldiyse (ya da zaten gelmişse)
        if (g_tcp.rxready) {
            uint16_t n = (g_tcp.rxlen > (uint16_t)m) ? (uint16_t)m : g_tcp.rxlen;
            memcpy(o, g_tcp.rxbuf, n);
            
            // KRİTİK: Bayrakları ve uzunluğu temizle ki sistem kilitlenmesin
            g_tcp.rxready = 0; 
            g_tcp.rxlen = 0;
            
            return n; // Buradan '186' dönmesi lazım!
        }

        // Eğer Python bağlantıyı kapattıysa (FIN) ve veri yoksa
        if (g_tcp.closed && !g_tcp.rxready) return 0;
    }
    return 0; // Timeout
}

int net_tcp_close(void) { return 1; }

int net_http_get_to_buf(uint32_t ip, uint16_t p, const char* path, char* out, int cap, int* olen) {
    if (!net_tcp_connect(ip, p)) return 0;

    char req[512];
    int req_len = ksprintf(req, "GET %s HTTP/1.0\r\nHost: 10.0.2.2\r\nConnection: close\r\n\r\n", path);
    net_tcp_send((uint8_t*)req, req_len);

    int total_received = 0;

    for (int spin = 0; spin < 50000000 && total_received < cap; spin++) {
        net_poll_once();

        if (g_tcp.rxready) {
            int take = (int)g_tcp.rxlen;
            if (take > cap - total_received) take = cap - total_received;
            memcpy(out + total_received, g_tcp.rxbuf, take);
            total_received += take;
            g_tcp.rxready = 0;
            g_tcp.rxlen = 0;
            spin = 0;
        }

        if (g_tcp.closed && !g_tcp.rxready) break;
    }

    if (olen) *olen = total_received;
    return (total_received > 0);
}