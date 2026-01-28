#include <kernel/kbd.h>

static const uint8_t trq_norm[128] = {
    0, 27,'1','2','3','4','5','6','7','8','9','0','*','-', '\b',
    '\t','q','w','e','r','t','y','u','i','o','p','g','u','\n',
    0, 'a','s','d','f','g','h','j','k','l',';','i',',', 0,
    '<','z','x','c','v','b','n','m','o','c','.', 0, '*', 0, ' '
};

static const uint8_t trq_shift[128] = {
    0, 27,'!','\'','^','+','%','&','/','(',')','=','?','_', '\b',
    '\t','Q','W','E','R','T','Y','U','I','O','P','G','U','\n',
    /* ... Diğer TR Karakterleri ... */
};

// Bu objeyi kbd_set_layout içinde kullanacağız
kbd_layout_t layout_trq = {
    .name = "trq",
    .normal = trq_norm,
    .shift = trq_shift
};