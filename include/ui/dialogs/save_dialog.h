#ifndef SAVE_DIALOG_H
#define SAVE_DIALOG_H

#include <stdint.h>
#include <stdbool.h>

/**
 * @brief Kaydetme işlemi onaylandığında çağrılacak fonksiyon tipi.
 */
typedef void (*save_callback_t)(const char* filename);

typedef struct {
    char title[32];          
    char buffer[64];         
    save_callback_t on_save; 
} save_dialog_t;

// --- Temel Fonksiyonlar ---

void save_dialog_show(const char* title, save_callback_t callback);
void save_dialog_draw(void);
void save_dialog_handle_key(uint16_t scancode, char c);

/**
 * @brief Fare etkileşimlerini işler (Dropdown, Butonlar, Liste seçimi).
 * @param mx Fare X koordinatı
 * @param my Fare Y koordinatı
 * @param clicked Sol tık basıldı mı? (pressed & 1)
 */
void save_dialog_handle_mouse(int mx, int my, bool clicked);

bool save_dialog_is_active(void);

#endif