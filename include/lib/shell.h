#ifndef SHELL_H
#define SHELL_H

#include <stdint.h>

void shell_init(void);

/* NEW: event-driven */
void shell_handle_scancode(uint16_t sc);
void shell_tick(void);

/* opsiyonel eski ayarlar kalsın istiyorsan */
void shell_set_username(const char* u);
void shell_set_hostname(const char* h);
void shell_set_cwd(const char* p);

#endif
