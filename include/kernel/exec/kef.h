#pragma once
#include <stdint.h>
#include <stddef.h>

#define KEF_MAGIC 0x3146454B  // 'K''E''F''1' little endian

typedef struct {
    uint32_t magic;
    uint16_t version;
    uint16_t flags;
    uint32_t entry_rva;
    uint32_t image_size;
    uint32_t bss_size;
    uint32_t reserved0;
    uint32_t reserved1;
    uint32_t pad;
} kef_header_t;

/* =========================
   KEF ABI (NEW)
   ========================= */

struct kvx_api; // forward
typedef struct kvx_api kvx_api_t;

typedef struct kvx_kef_app {
    void (*on_create)(const kvx_api_t* api);
    void (*on_draw)(const kvx_api_t* api);
    void (*on_key)(const kvx_api_t* api, uint16_t keyev);
    void (*on_mouse)(const kvx_api_t* api, int mx, int my, uint8_t pr, uint8_t rel, uint8_t btn);
    void (*on_destroy)(const kvx_api_t* api);
} kvx_kef_app_t;

/* entry: kernel -> app
   app vtbl doldurur, 0 dönerse OK */
typedef int (*kvx_kef_entry_fn_t)(const kvx_api_t* api, kvx_kef_app_t* out_vtbl);

// Basit “konsol test” gibi tek-shot çalıştırmak istersen
int kef_exec(const char* path);