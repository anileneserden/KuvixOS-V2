#ifndef DESKTOP_H
#define DESKTOP_H

#include <stdint.h>
#include <stdbool.h>

// Yapı tanımını buraya taşıyoruz
typedef struct { 
    int x, y; 
    char label[32];
    bool dragging;
    bool is_selected; // <-- YENİ: Toplu seçim için kriti
    int app_id; 
    // Varsa custom_icon pointer'ın:
    const uint8_t (*custom_icon)[20];
} desktop_icon_t;

// Global değişkenleri diğer dosyaların görebilmesi için 'extern' olarak tanımla
extern desktop_icon_t desktop_icons[];
extern int icon_count;

// Fonksiyon prototipleri
void ui_desktop_run(void);
void draw_desktop_icon(desktop_icon_t* icon, int mx, int my, int index);

#endif