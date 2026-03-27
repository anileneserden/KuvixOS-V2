#ifndef SHELL_H
#define SHELL_H

#include <stdint.h>

void shell_init(void);
void shell_tick(void);

void shell_handle_key(uint16_t key);

/* Geçici uyumluluk */
void shell_handle_scancode(uint16_t sc);

void shell_set_username(const char* u);
void shell_set_hostname(const char* h);
void shell_set_cwd(const char* p);

/* key debug */
void shell_set_key_debug(int enabled);
int  shell_get_key_debug(void);

#endif