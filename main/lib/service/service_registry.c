#include "service.h"

/* Desktop servisi */
int kv_desktop_start(void) { return 0; }
int kv_desktop_stop(void) { return 0; }
int kv_desktop_status(void) { return 0; }

kv_service_t g_services[] = {
    {
        .name = "kv_desktop",
        .desc = "KuvixOS Desktop UI",
        .start = kv_desktop_start,
        .stop  = kv_desktop_stop,
        .status = kv_desktop_status,   // istersen 0 yapabilirsin
        .state = SVC_STOPPED,
        .enabled = 0,
    },
};

int g_service_count = (int)(sizeof(g_services) / sizeof(g_services[0]));
