#ifndef UI_EDITOR_H
#define UI_EDITOR_H

#include <stdint.h>

void editor_open(const char* path);
void editor_init(void);
void editor_tick(void);
void editor_handle_key(uint16_t key);
int  editor_is_active(void);

#endif