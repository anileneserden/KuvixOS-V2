// kernel/drivers/input/keymaps/trq.c
#include <kernel/kbd.h>

// PS/2 Set1 make code index'leri (0x00..0x7F)
// 128 elemanlı tablo. Yoksa 0.

static const uint8_t trq_norm[128] = {
    [0x01] = 27,      // ESC
    [0x02] = '1',
    [0x03] = '2',
    [0x04] = '3',
    [0x05] = '4',
    [0x06] = '5',
    [0x07] = '6',
    [0x08] = '{',
    [0x09] = '[',
    [0x0A] = '}',
    [0x0B] = '0',
    [0x0C] = '*',
    [0x0D] = '-',
    [0x0E] = '\b',
    [0x0F] = '\t',

    [0x10] = 'q',
    [0x11] = 'w',
    [0x12] = 'e',
    [0x13] = 'r',
    [0x14] = 't',
    [0x15] = 'y',
    [0x16] = 'u',
    [0x17] = 0xFD,    // özel
    [0x18] = 'o',
    [0x19] = 'p',
    [0x1A] = 0xF0,    // özel
    [0x1B] = 0xFC,    // özel
    [0x1C] = '\n',

    [0x1E] = 'a',
    [0x1F] = 's',
    [0x20] = 'd',
    [0x21] = 'f',
    [0x22] = 'g',
    [0x23] = 'h',
    [0x24] = 'j',
    [0x25] = 'k',
    [0x26] = 'l',
    [0x27] = 0xFE,    // özel
    [0x28] = 'i',
    [0x29] = '<',

    [0x2B] = ',',     // sen böyle istemiştin
    [0x2C] = 'z',
    [0x2D] = 'x',
    [0x2E] = 'c',
    [0x2F] = 'v',
    [0x30] = 'b',
    [0x31] = 'n',
    [0x32] = 'm',
    [0x33] = 0xF6,    // özel
    [0x34] = 0xE7,    // özel
    [0x35] = '.',

    [0x39] = ' ',
    [0x56] = '<',     // TR: < > tuşu (çoğunlukla 0x56)
};

static const uint8_t trq_shift[128] = {
    [0x01] = 27,
    [0x02] = '!',
    [0x03] = '\'',
    [0x04] = '^',
    [0x05] = '+',
    [0x06] = '%',
    [0x07] = '&',
    [0x08] = '/',
    [0x09] = '(',
    [0x0A] = ')',
    [0x0B] = '=',
    [0x0C] = '?',
    [0x0D] = '_',
    [0x0E] = '\b',
    [0x0F] = '\t',

    [0x10] = 'Q',
    [0x11] = 'W',
    [0x12] = 'E',
    [0x13] = 'R',
    [0x14] = 'T',
    [0x15] = 'Y',
    [0x16] = 'U',
    [0x17] = 'I',
    [0x18] = 'O',
    [0x19] = 'P',
    [0x1A] = 0xD0,    // özel
    [0x1B] = 0xDC,    // özel
    [0x1C] = '\n',

    [0x1E] = 'A',
    [0x1F] = 'S',
    [0x20] = 'D',
    [0x21] = 'F',
    [0x22] = 'G',
    [0x23] = 'H',
    [0x24] = 'J',
    [0x25] = 'K',
    [0x26] = 'L',
    [0x27] = 0xDE,    // özel
    [0x28] = 0xDD,    // özel
    [0x29] = 0xE9,    // özel

    [0x2B] = ';',
    [0x2C] = 'Z',
    [0x2D] = 'X',
    [0x2E] = 'C',
    [0x2F] = 'V',
    [0x30] = 'B',
    [0x31] = 'N',
    [0x32] = 'M',
    [0x33] = 0xD6,    // özel
    [0x34] = 0xC7,    // özel
    [0x35] = ':',

    [0x39] = ' ',
    [0x56] = '>',     // Shift + <tuşu> => >
};

// AltGr: SADECE ASCII / tek byte güvenli karakterler
// (UTF-8 karakterleri (€, ₺, £, ←, ½...) şimdilik yazma)

static const uint8_t trq_altgr[128] = {
    // --- üst sıra ---
    [0x01] = 27,
    [0x02] = '>',   // AltGr+1
    [0x03] = '@',   // AltGr+2
    [0x04] = '#',   // AltGr+3
    [0x05] = '$',   // AltGr+4
    [0x06] = '^',   // AltGr+5
    [0x07] = '&',   // AltGr+6
    [0x08] = '{',   // AltGr+7
    [0x09] = '[',   // AltGr+8
    [0x0A] = ']',   // AltGr+9
    [0x0B] = '}',   // AltGr+0
    [0x0C] = '?',
    [0x0D] = '_',
    [0x0E] = '\b',
    [0x0F] = '\t',

    // --- harfler (Shift ile aynı kalabilir) ---
    [0x10] = 'Q',
    [0x11] = 'W',
    [0x12] = 'E',
    [0x13] = 'R',
    [0x14] = 'T',
    [0x15] = 'Y',
    [0x16] = 'U',
    [0x17] = 'I',
    [0x18] = 'O',
    [0x19] = 'P',
    [0x1A] = 0xD0,
    [0x1B] = 0xDC,
    [0x1C] = '\n',

    [0x1E] = 'A',
    [0x1F] = 'S',
    [0x20] = 'D',
    [0x21] = 'F',
    [0x22] = 'G',
    [0x23] = 'H',
    [0x24] = 'J',
    [0x25] = 'K',
    [0x26] = 'L',
    [0x27] = 0xDE,
    [0x28] = 0xDD,
    [0x29] = '<',   // AltGr + "  =>  <   (senin istediğin)

    // --- alt sıra ---
    [0x2B] = ';',
    [0x2C] = 'Z',
    [0x2D] = 'X',
    [0x2E] = 'C',
    [0x2F] = 'V',
    [0x30] = 'B',
    [0x31] = 'N',
    [0x32] = 'M',
    [0x33] = 0xD6,
    [0x34] = 0xC7,
    [0x35] = ':',

    [0x39] = ' ',
    [0x56] = '<',   // TR klavyedeki < > fiziksel tuşu
};

kbd_layout_t layout_trq = {
    .name   = "trq",
    .normal = trq_norm,
    .shift  = trq_shift,
    .altgr  = trq_altgr
};