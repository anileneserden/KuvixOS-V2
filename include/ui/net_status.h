#pragma once
#include <stdint.h>

typedef enum {
    NETS_DOWN = 0,
    NETS_LINK,
    NETS_LAN,
    NETS_INET
} net_state_t;

void net_status_init(void);
void net_status_tick(void);        // desktop tick veya topbar tick'te çağır
void net_status_force_check(void); // "Test now" butonu için

net_state_t net_status_get(void);
const char* net_status_text(void);