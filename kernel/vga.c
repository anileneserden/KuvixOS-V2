#include <stddef.h>
#include <stdint.h>

static uint16_t* const VGA_BUFFER = (uint16_t*)0xB8000;
static size_t row = 0;
static size_t col = 0;
static uint8_t color = 0x0F; // beyaz üstüne siyah

static inline uint16_t vga_entry(char c, uint8_t color) {
    return (uint16_t)c | (uint16_t)color << 8;
}

void vga_putc(char c) {
    if (c == '\n') {
        col = 0;
        row++;
        return;
    }

    VGA_BUFFER[row * 80 + col] = vga_entry(c, color);
    col++;

    if (col >= 80) {
        col = 0;
        row++;
    }
}

void vga_print(const char* str) {
    while (*str) {
        vga_putc(*str++);
    }
}
