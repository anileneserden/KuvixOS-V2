#pragma once
#include <stdint.h>

void net_init(void);

// statik config (BE = big-endian)
void net_set_ipv4(uint32_t ip_be, uint32_t mask_be, uint32_t gw_be);

// ping: 1 hedef, 1 deneme (başarılıysa 1, değilse 0)
int net_ping_ipv4(uint32_t dst_ip_be);

void net_get_ipv4(uint32_t* ip_be, uint32_t* mask_be, uint32_t* gw_be);

// --- TCP minimal API (tek bağlantı) ---
int net_tcp_connect(uint32_t dst_ip_be, uint16_t dst_port);
int net_tcp_send(const uint8_t* data, uint16_t len);
int net_tcp_recv(uint8_t* out, int maxlen, uint32_t spin_timeout);
int net_tcp_close(void);

int net_http_get_to_buf(uint32_t ip_be, uint16_t port, const char* path,
                        char* out, int out_cap, int* out_len);

// --- UDP minimal API ---
int net_udp_send(uint32_t dst_ip_be, uint16_t src_port, uint16_t dst_port,
                 const uint8_t* data, uint16_t len);

int net_udp_recv(uint16_t listen_port,
                 uint32_t* out_src_ip_be, uint16_t* out_src_port,
                 uint8_t* out, uint16_t out_max,
                 uint32_t spin_timeout);

// --- DNS ---
int net_dns_resolve_a(const char* host, uint32_t dns_ip_be, uint32_t* out_ip_be);