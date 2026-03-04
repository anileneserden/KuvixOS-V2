#pragma once
#include <stdint.h>

void xhci_debug_dump(uint32_t mmio);
void xhci_minimal_init(uint32_t mmio);

// Hotplug polling (CCS değişimini yakalar)
// return:
//   +N  -> port N takıldı
//   -N  -> port N çıkarıldı
//   0   -> değişiklik yok / xhci init değil
int xhci_poll_hotplug(void);

void xhci_set_global(uint32_t mmio);
uint32_t xhci_get_portsc(uint32_t port);
uint32_t xhci_get_max_ports(void);