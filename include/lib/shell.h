#pragma once
#include <stdint.h>

void shell_init(void);
void shell_tick(void);
void shell_handle_scancode(uint16_t ev);

void shell_set_username(const char* u);
void shell_set_hostname(const char* h);
void shell_set_cwd(const char* p);
const char* shell_get_cwd(void);