#include <kernel/vga.h>
#include <arch/x86/io.h>

static uint16_t* const VGA_BUFFER = (uint16_t*)0xB8000;
static size_t row = 0;
static size_t col = 0;
static uint8_t color = 0x0F; // Beyaz üstüne siyah

static inline uint16_t vga_entry(char c, uint8_t color) {
    return (uint16_t)c | (uint16_t)color << 8;
}

// EKLEDİĞİMİZ FONKSİYON:
void vga_init(void) {
    row = 0;
    col = 0;
    // Tüm ekranı boşluk karakteriyle temizleyelim
    for (size_t y = 0; y < 25; y++) {
        for (size_t x = 0; x < 80; x++) {
            const size_t index = y * 80 + x;
            VGA_BUFFER[index] = vga_entry(' ', color);
        }
    }
}

void vga_putc(char c) {
    // Backspace desteği (Shell için çok önemli!)
    if (c == '\b') {
        if (col > 0) {
            col--;
        } else if (row > 0) {
            row--;
            col = 79;
        }
        VGA_BUFFER[row * 80 + col] = vga_entry(' ', color);
        return;
    }

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
    
    // Basit bir scrolling (kaydırma) mantığı eklenebilir, 
    // şimdilik row taşarsa başa döner veya ekranda kalır.
    if (row >= 25) {
        row = 0; // Şimdilik basitçe başa dön
    }

    vga_update_cursor(col, row);
}

void vga_print(const char* str) {
    while (*str) {
        vga_putc(*str++);
    }
}

void vga_update_cursor(int x, int y) {
    uint16_t pos = y * 80 + x;

    // VGA kontrol portları üzerinden imleç konumu güncellenir
    outb(0x3D4, 0x0F);
    outb(0x3D5, (uint8_t)(pos & 0xFF));
    outb(0x3D4, 0x0E);
    outb(0x3D5, (uint8_t)((pos >> 8) & 0xFF));
}