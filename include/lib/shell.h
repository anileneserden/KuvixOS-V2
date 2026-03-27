#ifndef SHELL_H
#define SHELL_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void shell_init(void);

void shell_handle_key(uint16_t key);
void shell_tick(void);

void shell_set_username(const char* u);
void shell_set_hostname(const char* h);
void shell_set_cwd(const char* p);

#ifdef __cplusplus
}
#endif

#endif