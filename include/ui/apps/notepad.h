#ifndef NOTEPAD_H
#define NOTEPAD_H

#include <stdint.h>
#include <stdbool.h>

#define NOTEPAD_MAX_TEXT 4096

typedef struct {
    char text[NOTEPAD_MAX_TEXT];
    uint32_t cursor;
    int window_id;
    bool active;
} notepad_t;

void notepad_init(void);
void notepad_draw(void);
void notepad_handle_key(uint8_t scancode);

#endif