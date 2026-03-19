#pragma once
#include <stdint.h>

#define KEF_MINIMAL_MAGIC   "KEF0"
#define KEF_MINIMAL_VERSION 1

typedef struct kef_minimal_header {
    char     magic[4];     // "KEF0"
    uint32_t version;
    uint32_t window_w;
    uint32_t window_h;
    uint32_t title_len;
    uint32_t text_len;
} kef_minimal_header_t;

typedef struct kef_minimal_app {
    char title[128];
    char text[256];
    int  width;
    int  height;
} kef_minimal_app_t;

int kef_minimal_load_file(const char* path, kef_minimal_app_t* out);