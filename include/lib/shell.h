#ifndef SHELL_H
#define SHELL_H

#include <stdint.h>

void shell_init(void);
void shell_readline(char* buffer, int max_len);
void shell_set_username(const char* u);
void shell_set_hostname(const char* h);
void shell_set_cwd(const char* p);
void shell_tick(void);
void shell_handle_scancode(uint16_t sc);

#endif