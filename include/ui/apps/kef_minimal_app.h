#pragma once
#include <app/app.h>
#include <kernel/exec/kef_minimal.h>

typedef struct {
    int window_id;
    int loaded;
    kef_minimal_app_t app;
} kef_minimal_state_t;

extern const app_vtbl_t g_kef_minimal_vtbl;