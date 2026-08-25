#ifndef DE_API_H
#define DE_API_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#pragma pack(push, 1)

// Fare Durumu Yapısı
typedef struct {
    int x;
    int y;
    uint8_t left_button;
    uint8_t right_button;
    uint8_t middle_button;
} de_mouse_state_t;

// Çekirdek ile DE Uygulaması Arasındaki API Yapısı
typedef struct {
    int screen_width;
    int screen_height;

    // Temel Çizim İşlevleri
    void (*put_pixel)(int x, int y, uint32_t color);
    void (*draw_rect)(int x, int y, int w, int h, uint32_t color);
    void (*fill_round_rect)(int x, int y, int w, int h, int r, uint32_t color);
    void (*fill_round_rect4)(int x, int y, int w, int h, int rtl, int rtr, int rbl, int rbr, uint32_t color); // <--- Yeni eklendi
    void (*draw_text)(int x, int y, const char* text, uint32_t color);
    void (*clear_screen)(uint32_t color);
    void (*update_display)(void);

    // Girdi ve Sistem İşlevleri
    void (*get_mouse)(de_mouse_state_t* state);
    char (*get_key)(void);
    void (*get_time)(char* buffer);
    void (*log)(const char* msg);

    // KBI Dosya Çizim Fonksiyonu
    int (*render_kbi)(int x, int y, const char* filepath);

    int (*create_file)(const char* path, const char* content, uint32_t size);
    int (*read_file)(const char* path, char* buffer, uint32_t max_size);

    void (*dmg_union_replace)(int x1, int y1, int x2, int y2);

    int (*get_file_count)(const char* path);
    int (*get_file_name_at)(const char* path, int index, char* dest_name, int max_len);

    // Uygulama Çalıştırma ve Pencere İşlevleri
    int (*exec)(const char* path);
    int (*create_window)(int x, int y, int w, int h);

    // String Fonksiyonları
    int (*ksprintf)(char* str, unsigned long size, const char* format, ...);
    size_t (*strlen)(const char* str);
    int (*strcmp)(const char* s1, const char* s2);
    int (*strncmp)(const char* s1, const char* s2, size_t n);
    char* (*strrchr)(const char* s, int c);

    void (*exit_de)(void);
} DE_API;

#pragma pack(pop)

#ifdef __cplusplus
}
#endif

#endif // DE_API_H