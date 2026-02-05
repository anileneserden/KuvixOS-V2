#ifndef WM_H
#define WM_H

#include <stdint.h>
#include <ui/window/window.h>

#define WM_MAX_WINDOWS 16

// Forward declaration
struct app;
typedef struct app app_t;

typedef struct {
    int left, top, right, bottom;
} ui_padding_t;

void wm_init(void);
int  wm_add_window(int x, int y, int w, int h, const char* title, app_t* owner);
void wm_close_window(int idx);
int  wm_get_window(int win_id, ui_window_t* out);
void wm_set_title(int win_id, const char* title);
// include/ui/wm.h içine eklenecekler:
void wm_set_owner(int win_id, void* owner_app);
void wm_set_active(int win_id);
int  wm_get_active_id(void);
app_t* wm_get_active_owner(void);
void wm_handle_mouse(int mx, int my, uint8_t pressed, uint8_t released, uint8_t buttons);
void wm_handle_mouse_move(int mx, int my);
ui_rect_t wm_get_client_rect(int win_id);
void wm_toggle_maximize(int idx);
void wm_minimize(int idx);
void wm_draw(void);

int wm_find_window_at(int x, int y);

void wm_invalidate_window(int win_id);
int  wm_is_any_window_captured(void);

// Debug Fonksiyonları (wm.c'nin sonundaki hataları bitirir)
int      wm_dbg_dc_pending(void);
uint32_t wm_dbg_dc_age_ms(uint32_t now_ms);
int      wm_dbg_dc_win(void);

#endif