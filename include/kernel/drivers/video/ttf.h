#ifndef TTF_H
#define TTF_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

int ttf_is_initialized(void);
bool ttf_init_from_memory(uint8_t* font_buffer, size_t buffer_size, float pixel_height);
unsigned char* ttf_get_code_bitmap(uint32_t codepoint, int* w, int* h, int* xoff, int* yoff);
void ttf_ttf_free_bitmap(unsigned char* bitmap);

#endif