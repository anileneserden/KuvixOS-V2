// kernel/drivers/input/keymaps/trq.c
#include <kernel/kbd.h>

// Not: Bunlar PS/2 Set1 "make code" index'leri (0x00..0x7F).
// Diziler 128 elemandır. Verilmeyen index'ler otomatik 0 kalır.
// Karakter üretimi: sadece "key down" (released=0) anında yapılmalı.

static const uint8_t trq_norm[128] = {
    [0x01] = 27,      // ESC
    [0x02] = '1',
    [0x03] = '2',
    [0x04] = '3',
    [0x05] = '4',
    [0x06] = '5',
    [0x07] = '6',
    [0x08] = '7',     // 7
    [0x09] = '8',
    [0x0A] = '9',
    [0x0B] = '0',
    [0x0C] = '*',
    [0x0D] = '-',
    [0x0E] = '\b',    // Backspace
    [0x0F] = '\t',    // Tab

    [0x10] = 'q',
    [0x11] = 'w',
    [0x12] = 'e',
    [0x13] = 'r',
    [0x14] = 't',
    [0x15] = 'y',
    [0x16] = 'u',
    [0x17] = 0xFD,       // (senin özel kodun)
    [0x18] = 'o',
    [0x19] = 'p',
    [0x1A] = 0xF0,       // (senin özel kodun)
    [0x1B] = 0xFC,       // (senin özel kodun)
    [0x1C] = '\n',    // Enter

    [0x1E] = 'a',
    [0x1F] = 's',
    [0x20] = 'd',
    [0x21] = 'f',
    [0x22] = 'g',
    [0x23] = 'h',
    [0x24] = 'j',
    [0x25] = 'k',
    [0x26] = 'l',
    [0x27] = 0xFE,       // (senin özel kodun)
    [0x28] = 'i',
    [0x29] = '"',

    [0x2B] = ',',     // TRQ: sen böyle istemişsin
    [0x2C] = 'z',
    [0x2D] = 'x',
    [0x2E] = 'c',
    [0x2F] = 'v',
    [0x30] = 'b',
    [0x31] = 'n',
    [0x32] = 'm',
    [0x33] = 0xF6,       // (senin özel kodun)
    [0x34] = 0xE7,       // (senin özel kodun)
    [0x35] = '.',     // nokta

    [0x39] = ' ',     // Space
};

static const uint8_t trq_shift[128] = {
    [0x01] = 27,
    [0x02] = '!',
    [0x03] = '\'',
    [0x04] = '^',
    [0x05] = '+',
    [0x06] = '%',
    [0x07] = '&',
    [0x08] = '/',     // ✅ SHIFT+7 => '/'
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
    [0x1A] = 0xD0,       // (senin özel kodun)
    [0x1B] = 0xDC,      // (senin özel kodun)
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
    [0x27] = 0xDE,       // (senin özel kodun)
    [0x28] = 0xDD,     // burada 'İ' gibi özel istersen kendi kodunu koy
    [0x29] = 0xE9,       // istersen SHIFT+" için başka bir şey koy

    [0x2B] = ';',
    [0x2C] = 'Z',
    [0x2D] = 'X',
    [0x2E] = 'C',
    [0x2F] = 'V',
    [0x30] = 'B',
    [0x31] = 'N',
    [0x32] = 'M',
    [0x33] = 0xD6,      // (senin özel kodun)
    [0x34] = 0xC7,      // (senin özel kodun)
    [0x35] = ':',

    [0x39] = ' ',
};

kbd_layout_t layout_trq = {
    .name   = "trq",
    .normal = trq_norm,
    .shift  = trq_shift,
};
