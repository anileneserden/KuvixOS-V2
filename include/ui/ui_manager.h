#ifndef UI_MANAGER_H
#define UI_MANAGER_H

#include <stdint.h>

typedef enum {
    MODE_DESKTOP = 0,
    MODE_3D_RENDER = 1
} kernel_ui_mode_t;

// Global mod değişkeni
extern int g_current_mode;

// --- EKLENECEK SATIR ---
// Diğer dosyaların bu fonksiyonu tanıyabilmesi için prototip
void ui_manager_update(void); 

#endif