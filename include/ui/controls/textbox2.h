#pragma once
#include <ui/controls/control.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef TEXTBOX2_MAX
#define TEXTBOX2_MAX 256
#endif

typedef struct textbox2 {
    ui_control_t base;

    char text[TEXTBOX2_MAX];
    int  len;
    int  caret;

    bool focused;
    bool readonly;

    const char* hint;

    void (*on_enter)(struct textbox2* tb);
    void (*on_change)(struct textbox2* tb);
} textbox2_t;

void textbox2_init(
    textbox2_t* tb,
    int id,
    ui_point_t loc,
    ui_size_t size
);

const char* textbox2_get_text(textbox2_t* tb);
void textbox2_set_text(textbox2_t* tb, const char* s);

#ifdef __cplusplus
}
#endif