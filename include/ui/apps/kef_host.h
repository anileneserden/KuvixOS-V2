#pragma once
#include <stdint.h>
#include <app/app.h>
#include <kernel/exec/kef_api.h>

typedef struct kef_host {
    char path[256];

    // loaded image base
    uint8_t* img;

    // vtbl kernel tarafında tutulur (range problemi yok)
    kvx_kef_app_t vtbl;
    int vtbl_ready;
} kef_host_t;

extern const app_vtbl_t kef_host_vtbl;