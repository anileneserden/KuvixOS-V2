// src/lib/ui/input.h
#pragma once

#include <stdint.h>
#include <stdbool.h>

//
// 1) Basit text input kutusu (mevcut sistemin)
//
typedef struct {
    int x;          // piksel olarak sol üst x
    int y;          // piksel olarak sol üst y
    int w;          // genişlik (piksel)
    int h;          // yükseklik (piksel)

    const char* label;   // "Username", "Password" gibi başlık
    char* buffer;        // metnin tutulduğu yer
    int  max_len;        // buffer boyutu
    int  len;            // şu anki karakter sayısı

    int  has_focus;      // ileride input için kullanacağız
    int  hidden;         // şifre kutusu ise 1 -> ekranda *** göster
} ui_input_t;

// Textbox çizimi
void ui_input_draw(const ui_input_t* in);


//
// 2) Genel input event sistemi (klavye + mouse)
//

typedef enum {
    INPUT_NONE = 0,
    INPUT_KEY_DOWN,
    INPUT_KEY_UP,
    INPUT_MOUSE_MOVE,
    INPUT_MOUSE_BUTTON_DOWN,
    INPUT_MOUSE_BUTTON_UP,
} input_type_t;

typedef struct {
    uint8_t shift : 1;
    uint8_t ctrl  : 1;
    uint8_t alt   : 1;
} key_mods_t;

typedef struct {
    input_type_t type;

    union {
        struct {
            uint8_t scancode;
            char    ch;      // çözümlenmiş karakter (şimdilik 0 da olabilir)
            key_mods_t mods;
        } key;

        struct {
            int dx;
            int dy;
            int x;           // absolute pozisyon (cursor için)
            int y;
            uint8_t buttons; // bit0=left, bit1=right, bit2=middle
        } mouse;
    };
} input_event_t;

// Event kuyruğu API'si
void input_system_init(void);
void input_push_event(const input_event_t* ev);
bool input_pop_event(input_event_t* ev);
