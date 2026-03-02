#pragma once
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void inputtest_init(void);
void inputtest_tick(void);   // sürekli çağır
void inputtest_draw(void);   // ekranı çiz
void inputtest_handle_scancode(uint16_t sc);

#ifdef __cplusplus
}
#endif