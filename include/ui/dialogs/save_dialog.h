#ifndef SAVE_DIALOG_H
#define SAVE_DIALOG_H

#include <stdint.h>
#include <stdbool.h>

// Kaydetme işlemi bittiğinde çağrılacak fonksiyon tipi
typedef void (*save_callback_t)(const char* filename);

typedef struct {
    char title[32];
    char buffer[64];      // Yazılan dosya adı
    const char* data;     // Uygulamadan gelen veri
    uint32_t data_size;   // Verinin boyutu
    save_callback_t on_save;
} save_dialog_t;

// --- Dışarıya Açık Fonksiyonlar ---
void save_dialog_show(const char* title, const char* data, uint32_t size, save_callback_t callback);
void save_dialog_draw(void);
void save_dialog_handle_mouse(int mx, int my, bool clicked);
void save_dialog_handle_key(uint16_t scancode, char c);
bool save_dialog_is_active(void);
void save_dialog_refresh(void);

#endif