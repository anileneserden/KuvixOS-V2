#pragma once
#include <stdint.h>
#include <stddef.h>

#define KEF_MAGIC 0x3146454B  // 'K''E''F''1' little endian

typedef struct kef_header_t {
    uint32_t magic;       // KEF_MAGIC
    uint16_t version;     // 1
    uint16_t flags;       // 0
    uint32_t entry_rva;   // entry offset from image base
    uint32_t image_size;  // bytes after header
    uint32_t bss_size;    // zeroed bytes after image
    uint32_t reserved0;
    uint32_t reserved1;
} kef_header_t;

// Şimdilik minimal API
typedef struct kvx_api_t {
    void (*log)(const char* s);
} kvx_api_t;

typedef int (*kef_entry_fn_t)(const kvx_api_t* api);

int kef_exec(const char* path);