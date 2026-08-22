#ifndef LOGIN_API_H
#define LOGIN_API_H

#include <stdint.h>

// Fare durumu yapısı (DE API ile uyumlu)
typedef struct {
    int x;
    int y;
    uint8_t left_button;
    uint8_t right_button;
    uint8_t middle_button;
} login_mouse_state_t;

// Login Ekranı için API Yapısı
typedef struct {
    int screen_width;
    int screen_height;
    
    // Temel Çizim ve Ekran Fonksiyonları
    void (*put_pixel)(int x, int y, uint32_t color);
    void (*draw_rect)(int x, int y, int w, int h, uint32_t color);
    void (*draw_text)(int x, int y, const char* text, uint32_t color);
    void (*clear_screen)(uint32_t color);
    void (*update_display)(void);

    // Girdi ve Sistem Fonksiyonları
    void (*get_mouse)(login_mouse_state_t* state);
    char (*get_key)(void);
    void (*get_time)(char* buffer);
    void (*log)(const char* msg);

    // Dosya İşlemleri (Gerekirse kullanıcı listesi vb. okumak için)
    int (*read_file)(const char* path, char* buffer, uint32_t max_size);

    int (*render_kbi)(int target_x, int target_y, const char* filepath);
} LoginAPI;

// Sadece C++ derleyicisi derlerken extern "C" kullanılsın
#ifdef __cplusplus
extern "C" {
#endif

LoginAPI* get_login_api();

#ifdef __cplusplus
}
#endif

#endif