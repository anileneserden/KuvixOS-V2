#ifndef NOTEPAD_H
#define NOTEPAD_H

#include <stdint.h>
#include <stdbool.h>

#define NOTEPAD_MAX_TEXT 4096

typedef struct {
    char text[NOTEPAD_MAX_TEXT];
    char file_path[128]; // Hangi dosya açık?
    uint32_t cursor;
    int window_id;
    bool active;
    bool menu_open;      // Menü açık mı?
    bool is_dirty;       // Değişiklik var mı? (*)
} notepad_t;

void notepad_init(void);
void notepad_draw(void);
void notepad_handle_key(uint8_t scancode);
void notepad_open_file(const char* path);

#endif