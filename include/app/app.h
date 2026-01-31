// src/lib/app/app.h
#pragma once
#include <stdint.h>

// ✅ Forward typedef (doğru olan)
typedef struct app app_t;

typedef struct app_vtbl {
    void (*on_create)(app_t* a);
    void (*on_destroy)(app_t* a);

    void (*on_mouse)(app_t* a, int mx, int my,
                     uint8_t pressed, uint8_t released, uint8_t buttons);

    void (*on_key)(app_t* a, uint16_t keyev);

    void (*on_update)(app_t* a);
    void (*on_draw)(app_t* a);
} app_vtbl_t;

// ✅ App instance
struct app {
    int win_id;
    void* user;
    const app_vtbl_t* v;

    // demo_app.c içinde .name kullanıyorsan bunu ekle:
    // const char* name;
};
