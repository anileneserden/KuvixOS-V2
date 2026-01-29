#include <kernel/vga.h>
#include <arch/x86/io.h>
#include <stdint.h>

// Eğer io.h içindeki outb bazen sorun çıkarıyorsa garantiye alalım
#ifndef outb
#define outb(port, val) __asm__ volatile ("outb %b0, %w1" : : "a"(val), "Nd"(port))
#endif

static uint16_t* const VGA_BUFFER = (uint16_t*)0xB8000;
static size_t row = 0;
static size_t col = 0;
uint8_t color = 0x0F; // Beyaz üstüne siyah

/* --- Fonksiyon Prototipleri --- */
// Derleyiciye bu fonksiyonun aşağıda olduğunu önceden bildiriyoruz
void vga_update_cursor(int x, int y);

static inline uint16_t vga_entry(char c, uint8_t color) {
    return (uint16_t)c | (uint16_t)color << 8;
}

/* --- Fonksiyon Tanımları --- */

void vga_init(void) {
    row = 0;
    col = 0;
    // Ekranı temizle
    for (size_t y = 0; y < 25; y++) {
        for (size_t x = 0; x < 80; x++) {
            VGA_BUFFER[y * 80 + x] = vga_entry(' ', color);
        }
    }

    // İmleci (Cursor) donanımsal olarak etkinleştir
    outb(0x3D4, 0x0A);
    outb(0x3D5, (inb(0x3D5) & 0xC0) | 0);
    outb(0x3D4, 0x0B);
    outb(0x3D5, (inb(0x3D5) & 0xE0) | 15);

    vga_update_cursor(0, 0);
}

void vga_update_cursor(int x, int y) {
    uint16_t pos = y * 80 + x;

    outb(0x3D4, 0x0F);
    outb(0x3D5, (uint8_t)(pos & 0xFF));
    outb(0x3D4, 0x0E);
    outb(0x3D5, (uint8_t)((pos >> 8) & 0xFF));
}

void vga_putc(char c) {
    if (c == '\b') {
        // Backspace: Bir geri git ve orayı boşalt
        if (col > 0) {
            col--;
        } else if (row > 0) {
            row--;
            col = 79;
        }
        VGA_BUFFER[row * 80 + col] = vga_entry(' ', color);
    } 
    else if (c == '\n') {
        col = 0;
        row++;
    } 
    else {
        VGA_BUFFER[row * 80 + col] = vga_entry(c, color);
        col++;
    }

    // Satır sonu kontrolü
    if (col >= 80) {
        col = 0;
        row++;
    }

    // Scrolling (Basit kaydırma)
    if (row >= 25) {
        row = 24; // Şimdilik en alt satırda kalalım
        // İleride tüm ekranı yukarı kaydıran memmove eklenebilir
    }

    vga_update_cursor(col, row);
}

void vga_print(const char* str) {
    while (*str) {
        vga_putc(*str++);
    }
}

void vga_clear(void) {
    uint16_t *vga_buffer = (uint16_t *)0xB8000;
    uint16_t empty_char = (0x0F << 8) | ' '; 

    for (int i = 0; i < 80 * 25; i++) {
        vga_buffer[i] = empty_char;
    }
    
    // İmleci ve koordinatları en başa çekiyoruz
    row = 0;
    col = 0;
    vga_update_cursor(0, 0);
}

void vga_set_color(uint8_t new_color) {
    color = new_color;
}