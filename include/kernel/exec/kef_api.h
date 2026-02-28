#pragma once
#include <stdint.h>
#include <stdbool.h>

typedef struct kvx_api {
    void (*log)(const char* s);
    void (*fill_rect)(int x, int y, int w, int h, uint32_t color);
    void (*text)(int x, int y, uint32_t color, const char* s);
    void (*invalidate)(void);
    void (*create_label)(int x, int y, const char* text);
    void (*create_button)(int x, int y, int w, int h,
                        const char* text,
                        void (*on_click)(void*),
                        void* user);
} kvx_api_t;

typedef struct kvx_kef_app {
    void (*on_create)(const kvx_api_t* api);
    void (*on_draw)(const kvx_api_t* api);
    void (*on_key)(const kvx_api_t* api, uint16_t keyev);
    void (*on_mouse)(const kvx_api_t* api, int mx, int my, uint8_t pr, uint8_t rel, uint8_t btn);
    void (*on_destroy)(const kvx_api_t* api);
} kvx_kef_app_t;

/* ✅ YENİ ABI:
   entry vtbl'i out param'a yazar, int döndürür (0 = OK)
*/
typedef int (*kvx_kef_entry_fn_t)(const kvx_api_t* api, kvx_kef_app_t* out_vtbl);

extern const kvx_api_t g_kvx_api;
void kef_api_set_active(int on);
int  kef_api_is_active(void);