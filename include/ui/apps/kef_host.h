#pragma once
#include <stdint.h>
#include <app/app.h>
#include <kernel/exec/kef_api.h>
#include <ui/widget.h>

#define KEF_MAX_WIDGETS 32

typedef struct kef_host {
    char path[256];

    // loaded image base
    uint8_t* img;

    // vtbl kernel tarafında tutulur
    kvx_kef_app_t vtbl;
    int vtbl_ready;

    // ✅ Widget state (her pencere için ayrı)
    widget_t widgets[KEF_MAX_WIDGETS];
    int widget_count;
} kef_host_t;

extern const app_vtbl_t kef_host_vtbl;

// kef_api.c tarafı aktif host'u buradan alacak
kef_host_t* kef_host_get_active(void);