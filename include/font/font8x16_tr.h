#ifndef FONT8X16_TR_H
#define FONT8X16_TR_H

#include <stdint.h>

// B makrosunu buraya koyuyoruz ki her yerde kullanabilelim
#ifndef B
#define B(x) ((uint8_t)0b##x)
#endif

// 256 karakter, her biri 16 satır
extern const uint8_t font8x16_tr[256][16];

#endif