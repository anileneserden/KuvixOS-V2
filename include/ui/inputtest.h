#pragma once
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void inputtest_init(void);
void inputtest_tick(void);   // sürekli çağır
void inputtest_draw(void);   // ekranı çiz

#ifdef __cplusplus
}
#endif