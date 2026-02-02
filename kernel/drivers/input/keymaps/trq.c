#include <kernel/kbd.h>

static const uint8_t trq_norm[128] = {
    0,  27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '*', '-', '\b',
    /* 0x0F */ '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 3, 'o', 'p', 1, 6, '\n', 
    /* 0x1D */ 0, 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', 2, 'i', '"', 
    /* 0x2A */ 0, ',', 'z', 'x', 'c', 'v', 'b', 'n', 'm', 4, 5, '.', 0, '*', 0, ' ' 
};

static const uint8_t trq_shift[128] = {
    0,  27, '!', '\'', '^', '+', '%', '&', '/', '(', ')', '=', '?', '_', '\b',
    '\t', 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', 7, 12, '\n',  // 7=Ğ, 12=Ü
    0, 'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', 8, 'i', 0, 
    0, ';', 'Z', 'X', 'C', 'V', 'B', 'N', 'M', 10, 11, ':', 0, '*', 0, ' '
};

// kernel/drivers/input/keymaps/trq.c en altı
kbd_layout_t layout_trq = {
    .name = "trq",
    .normal = trq_norm,
    .shift = trq_shift
};