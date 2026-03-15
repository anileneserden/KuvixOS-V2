#pragma once
#include <stdint.h>

int  e1000_probe(uint8_t bus, uint8_t slot, uint8_t func);

// hazır mı?
int  e1000_is_ready(void);

// MAC al
void e1000_get_mac(uint8_t out_mac[6]);

// Ethernet frame gönder (dst + ethertype + payload)
int  e1000_send_eth(const uint8_t dst_mac[6], uint16_t ethertype_be,
                    const uint8_t* payload, uint16_t payload_len);

// RX: 1 paket çek (varsa). out_len setlenir. 0=paket yok, 1=paket var
int  e1000_rx_pop(uint8_t* out_frame, uint16_t out_max, uint16_t* out_len);