#pragma once

#include <stdint.h>

typedef struct {
    char name[256];
    uint32_t compressed_size;
    uint32_t uncompressed_size;
    uint32_t local_header_offset;
    uint16_t method;
    uint16_t flags;
    uint32_t crc32;
    int is_dir;
} zip_entry_t;

typedef int (*zip_entry_callback_t)(const zip_entry_t* entry, void* user);

int zip_list_entries(const char* path, zip_entry_callback_t cb, void* user);