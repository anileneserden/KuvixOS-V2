// src/lib/font/font8x16_basic.c
#include <ui/font/font8x16_basic.h>
#include <ui/font/font8x8_basic.h>
#include <stdint.h>

static inline uint8_t row8(uint8_t ch, int r8) {
    const uint8_t* g = font8x8_basic[ch];
    return (r8 < 0 || r8 > 7) ? 0 : g[r8];
}

// Overlay bitmask'leri (bit7 soldaki piksel)
#define BREVE_ROW0 0x18  //   **
#define BREVE_ROW1 0x24  //  *  *
#define CEDIL_ROW  0x18  //   **  (basit cedilla)

uint8_t font8x16_basic_row(uint8_t ch, int row) {
    if (row < 0) row = 0;
    if (row > 15) row = 15;

    // Default: 8x8'i 2x dikey büyüt
    uint8_t base = row8(ch, row >> 1);

    // --- ğ (0xF0): g + breve ---
    if (ch == 0xF0) {
        uint8_t line = row8('g', row >> 1);

        // breve'yi en üst 2 satıra overlay et
        if (row == 0) line |= BREVE_ROW0;
        if (row == 1) line |= BREVE_ROW1;

        return line;
    }

    // --- Ğ (0xD0): G + breve ---
    if (ch == 0xD0) {
        uint8_t line = row8('G', row >> 1);

        if (row == 0) line |= BREVE_ROW0;
        if (row == 1) line |= BREVE_ROW1;

        return line;
    }

    // ü (0xFC)
    if (ch == 0xFC) {

        if (row == 0) return 0x24;
        if (row == 1) return 0x00;

        if (row == 2) {
            uint8_t line = row8('u', 0);
            line &= 0x3C;
            return line;
        }

        return row8('u', row >> 1);
    }

    // Ü (0xDC)
    if (ch == 0xDC) {

        // 1️⃣ Umlaut
        if (row == 0) return 0x24; //  *  *
        if (row == 1) return 0x00;

        // 2️⃣ U'nun üst satırını kırp
        if (row == 2) {
            uint8_t line = row8('U', 0);
            // soldan 2, sağdan 2 piksel sil
            line &= 0b00111100;  // 0x3C
            return line;
        }

        // 3️⃣ geri kalan normal
        return row8('U', row >> 1);
    }

    // --- ç (0xE7): c + cedilla ---
    if (ch == 0xE7) {
        uint8_t line = row8('c', row >> 1);

        // cedilla: en altlara ekle
        if (row == 14) line |= CEDIL_ROW;
        if (row == 15) line |= 0x10; // küçük uzama hissi

        return line;
    }

    // --- Ç (0xC7): C + cedilla ---
    if (ch == 0xC7) {
        uint8_t line = row8('C', row >> 1);

        if (row == 14) line |= CEDIL_ROW;
        if (row == 15) line |= 0x10;

        return line;
    }

    // --- é (0xE9): e + acute accent (´) ---
    if (ch == 0xE9) {
        uint8_t line = row8('e', row >> 1);

        // acute accent (sağa yatık)
        if (row == 0) line |= 0x30; //  ** 
        if (row == 1) line |= 0x18; //   **

        return line;
    }

    // diğerleri: normal scaled
    return base;
}