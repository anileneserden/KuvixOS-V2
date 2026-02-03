// src/lib/ui/ui_button/ui_button.h
#pragma once

#include <stdint.h>

typedef struct {
    int x;
    int y;
    int w;
    int h;

    const char* label;

    int is_hover;
    int is_pressed;
} ui_button_t;

// x, y, w, h, label ile butonu hazırla
void ui_button_init(ui_button_t* btn,
                    int x, int y, int w, int h,
                    const char* label);

// Her frame çağrılır, mouse'a göre state günceller
// Bu frame'de tıklanmışsa (press+release içerde) 1 döner, yoksa 0
int ui_button_update(ui_button_t* btn);

// Çizim
void ui_button_draw(const ui_button_t* btn);
