#pragma once

#include <kvx_c/kuvixos.h>

#ifdef __cplusplus
extern "C" {
#endif

void kvx_sdk_init(const kvx_api_t* api);
void print(const char* text);
int kvx_argc(void);
const char* kvx_argv(int index);

#ifdef __cplusplus
}
#endif