#pragma once
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum kvx_app_kind {
    KVX_APP_KIND_WINDOW = 1,
    KVX_APP_KIND_CONSOLE = 2,
} kvx_app_kind_t;

typedef struct kvx_api {
    void (*fill_rect)(int x, int y, int w, int h, uint32_t color);
    void (*text)(int x, int y, uint32_t color, const char* s);
    void (*print)(const char* s);
    int (*arg_count)(void);
    const char* (*arg_at)(int index);
} kvx_api_t;

typedef struct kvx_kef_app {
    uint32_t kind;
    void (*on_draw)(const kvx_api_t* api);

    /* embedded ui json */
    const char* ui_json;
    uint32_t ui_json_size;
} kvx_kef_app_t;

#ifdef __cplusplus
}
#endif