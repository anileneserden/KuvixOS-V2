#ifndef VGA_H
#define VGA_H

#include <stddef.h>
#include <stdint.h>

void vga_init(void);
void vga_putc(char c);
void vga_print(const char* str);
void vga_clear(void);

// Bunu ekle:
void vga_set_color(uint8_t color);

#endif