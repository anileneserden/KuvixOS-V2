#ifndef DE_API_H
#define DE_API_H

#include <stdint.h>
#include <stdbool.h>

#pragma pack(push, 1)

typedef struct {
    int x;
    int y;
    uint8_t left_button;
    uint8_t right_button;
    uint8_t middle_button;
} de_mouse_state_t;

typedef struct {
    int screen_width;
    int screen_height;

    // Temel Çizim İşlevleri
    void (*put_pixel)(int x, int y, uint32_t color);
    void (*draw_rect)(int x, int y, int w, int h, uint32_t color);
    void (*draw_text)(int x, int y, const char* text, uint32_t color);
    void (*clear_screen)(uint32_t color);
    void (*update_display)(void);

    // Girdi ve Sistem İşlevleri
    void (*get_mouse)(de_mouse_state_t* state);
    char (*get_key)(void);
    void (*get_time)(char* buffer);
    void (*log)(const char* msg);
} DE_API;

#pragma pack(pop)

#endif // DE_API_H