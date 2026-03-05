#ifndef TUI_INPUT_H
#define TUI_INPUT_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// input ekranını başlatır
// path: yazılacak config dosyası (örn /system/user.conf)
// key : yazılacak anahtar (örn username)
// title: ekranda gösterilecek başlık (örn "Set Username")
// next_action: ENTER sonrası çalıştırılacak action (örn "cfg:/system/tui/main.cfg")
// initial: textbox başlangıç değeri (opsiyonel)
void tui_input_begin(const char* title,
                     const char* path,
                     const char* key,
                     const char* next_action,
                     const char* initial);

int  tui_input_is_active(void);
void tui_input_draw(void);
void tui_input_tick(void);
void tui_input_handle_scancode(uint16_t sc);

#ifdef __cplusplus
}
#endif

#endif