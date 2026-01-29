#ifndef VGA_H
#define VGA_H

#include <stddef.h>
#include <stdint.h>

// Ekranı temizler ve imleç değişkenlerini sıfırlar
void vga_init(void);

// Tek bir karakter basar (satır sonu kontrolü dahil)
void vga_putc(char c);

// Null-terminated string basar
void vga_print(const char* str);

void vga_clear(void);

#endif