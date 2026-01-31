#include <kernel/kbd.h>

/* US Klavye Haritası */
static const uint8_t us_norm[128] = {
    0, 27,'1','2','3','4','5','6','7','8','9','0','-','=', '\b',
    '\t','q','w','e','r','t','y','u','i','o','p','[',']','\n',
    0, 'a','s','d','f','g','h','j','k','l',';','\'','`', 0,
    '\\','z','x','c','v','b','n','m',',','.','/', 0, '*', 0, ' '
};

static const uint8_t us_shift[128] = {
    0, 27,'!','@','#','$','%','^','&','*','(',')','_','+', '\b',
    '\t','Q','W','E','R','T','Y','U','I','O','P','{','}','\n',
    0, 'A','S','D','F','G','H','J','K','L',':','"','~', 0,
    '|','Z','X','C','V','B','N','M','<','>','?', 0, '*', 0, ' '
};

// layout.c'nin extern ile aradığı değişken tam olarak bu:
kbd_layout_t layout_us = {
    .name = "us",
    .normal = us_norm,
    .shift = us_shift
};