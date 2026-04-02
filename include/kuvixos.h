#pragma once

#include <kvx_c/kuvixos.h>

#ifdef __cplusplus
extern "C" {
#endif

void kvx_sdk_init(const kvx_api_t* api);
void print(const char* text);
int kvx_argc(void);
const char* kvx_argv(int index);
int kvx_file_read_all(const char* path, char* out, uint32_t cap, uint32_t* out_len);

#ifdef __cplusplus
}
#endif