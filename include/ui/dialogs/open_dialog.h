#ifndef OPEN_DIALOG_H
#define OPEN_DIALOG_H

#include <stdint.h>
#include <stdbool.h>

typedef void (*open_callback_t)(const char* full_path);

typedef struct {
    char title[32];
    char buffer[64];
    open_callback_t on_open;
    int owner_win_id;
    bool pick_dir;
} open_dialog_t;

void open_dialog_show_dirpicker(const char* title, const char* initial_path, int owner_win_id, open_callback_t cb);
void open_dialog_show(const char* title, const char* initial_name, int owner_win_id, open_callback_t cb);
void open_dialog_draw(void);
void open_dialog_handle_mouse(int mx, int my, bool clicked);
void open_dialog_handle_key(uint16_t scancode, char c);
void open_dialog_handle_wheel(int step);
void open_dialog_handle_mouse_move(int mx, int my, uint8_t btns);

bool open_dialog_is_active(void);
void open_dialog_refresh(void);
int  open_dialog_get_owner_win_id(void);

#endif
