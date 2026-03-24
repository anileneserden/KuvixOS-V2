#pragma once
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct kvx_api {
    void (*fill_rect)(int x, int y, int w, int h, uint32_t color);
    void (*text)(int x, int y, uint32_t color, const char* s);
} kvx_api_t;

typedef struct kvx_kef_app {
    void (*on_draw)(const kvx_api_t* api);

    /* embedded ui json */
    const char* ui_json;
    uint32_t ui_json_size;
} kvx_kef_app_t;

#ifdef __cplusplus
}
#endif