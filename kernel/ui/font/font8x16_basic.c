// src/lib/font/font8x16_basic.c
#include <ui/font8x16_basic.h>
#include <ui/font8x8_basic.h>

static inline uint8_t row8(uint8_t ch, int r8) {
    const uint8_t* g = font8x8_basic[ch];
    return (r8 < 0 || r8 > 7) ? 0 : g[r8];
}

// overlay bitmask'leri (bit7 soldaki piksel)
#define BREVE_ROW0 0x18  //   **
#define BREVE_ROW1 0x24  //  *  *
#define CEDIL_ROW  0x18  //   **   (basit cedilla)

uint8_t font8x16_basic_row(uint8_t ch, int row) {
    if (row < 0) row = 0;
    if (row > 15) row = 15;

    // Default: 8x8'i 2x dikey büyüt
    uint8_t base = row8(ch, row >> 1);

    // --- ğ (0xF0): g + breve ---
    if (ch == 0xF0) {
        uint8_t line = row8('g', row >> 1);
        if (row == 0) line |= BREVE_ROW0;
        if (row == 1) line |= BREVE_ROW1;
        return line;
    }

    // --- Ğ (0xD0): G + breve (istersen) ---
    if (ch == 0xD0) {
        uint8_t line = row8('G', row >> 1);
        if (row == 0) line |= BREVE_ROW0;
        if (row == 1) line |= BREVE_ROW1;
        return line;
    }

    // --- ç (0xE7): c + cedilla ---
    if (ch == 0xE7) {
        uint8_t line = row8('c', row >> 1);
        // cedilla'yı en altlara ekle (row 14-15 gibi)
        if (row == 14) line |= CEDIL_ROW;
        if (row == 15) line |= 0x10; // tek piksel aşağı uzama hissi
        return line;
    }

    // --- Ç (0xC7): C + cedilla ---
    if (ch == 0xC7) {
        uint8_t line = row8('C', row >> 1);
        if (row == 14) line |= CEDIL_ROW;
        if (row == 15) line |= 0x10;
        return line;
    }

    // diğerleri: normal scaled
    return base;
}