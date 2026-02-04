#include <kernel/kbd.h>

// Türkçe Q Klavye - Normal Mod (Küçük Harfler ve Sayılar)
static const uint8_t trq_norm[128] = {
    [0x01] = 27,    // ESC
    [0x02] = '1', [0x03] = '2', [0x04] = '3', [0x05] = '4', [0x06] = '5',
    [0x07] = '6', [0x08] = '7', [0x09] = '8', [0x0a] = '9', [0x0b] = '0',
    [0x0c] = '*', [0x0d] = '-', 
    [0x0e] = '\b',  // Backspace
    [0x0f] = '\t',  // Tab
    [0x10] = 'q', [0x11] = 'w', [0x12] = 'e', [0x13] = 'r', [0x14] = 't',
    [0x15] = 'y', [0x16] = 'u', [0x17] = 'i', [0x18] = 'o', [0x19] = 'p',
    [0x1a] = 3,     // ğ (Sayısal kodun 3 ise)
    [0x1b] = 1,     // ü (Sayısal kodun 1 ise)
    [0x1c] = '\n',  // Enter
    [0x1d] = 0,     // Sol Ctrl
    [0x1e] = 'a',   // A TUŞU (Kesinleşti)
    [0x1f] = 's', [0x20] = 'd', [0x21] = 'f', [0x22] = 'g', [0x23] = 'h',
    [0x24] = 'j', [0x25] = 'k', [0x26] = 'l', 
    [0x27] = 2,     // ş
    [0x28] = 6,     // i
    [0x29] = '"', 
    [0x2a] = 0,     // Sol Shift
    [0x2b] = ',', 
    [0x2c] = 'z', [0x2d] = 'x', [0x2e] = 'c', [0x2f] = 'v', [0x30] = 'b',
    [0x31] = 'n', [0x32] = 'm', 
    [0x33] = 4,     // ö
    [0x34] = 5,     // ç
    [0x35] = '.', 
    [0x39] = ' '    // Space
};

// Türkçe Q Klavye - Shift Modu (Büyük Harfler ve Semboller)
static const uint8_t trq_shift[128] = {
    [0x01] = 27,
    [0x02] = '!', [0x03] = '\'', [0x04] = '^', [0x05] = '+', [0x06] = '%',
    [0x07] = '&', [0x08] = '/',  [0x09] = '(', [0x0a] = ')', [0x0b] = '=',
    [0x0c] = '?', [0x0d] = '_', 
    [0x0e] = '\b',
    [0x0f] = '\t',
    [0x10] = 'Q', [0x11] = 'W', [0x12] = 'E', [0x13] = 'R', [0x14] = 'T',
    [0x15] = 'Y', [0x16] = 'U', [0x17] = 'I', [0x18] = 'O', [0x19] = 'P',
    [0x1a] = 7,     // Ğ
    [0x1b] = 12,    // Ü
    [0x1c] = '\n',
    [0x1e] = 'A', [0x1f] = 'S', [0x20] = 'D', [0x21] = 'F', [0x22] = 'G',
    [0x23] = 'H', [0x24] = 'J', [0x25] = 'K', [0x26] = 'L', 
    [0x27] = 8,     // Ş
    [0x28] = 'i', 
    [0x2c] = 'Z', [0x2d] = 'X', [0x2e] = 'C', [0x2f] = 'V', [0x30] = 'B',
    [0x31] = 'N', [0x32] = 'M', 
    [0x33] = 10,    // Ö
    [0x34] = 11,    // Ç
    [0x35] = ':',
    [0x39] = ' '
};

kbd_layout_t layout_trq = {
    .name = "trq",
    .normal = trq_norm,
    .shift = trq_shift
};