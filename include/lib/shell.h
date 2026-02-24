#ifndef SHELL_H
#define SHELL_H

#include <stdint.h>

void shell_init(void);
void shell_tick(void);
void shell_handle_scancode(uint16_t sc);

void shell_begin_heredoc(const char* path, const char* endtok);

/* opsiyonel eski ayarlar kalsın istiyorsan */
void shell_set_username(const char* u);
void shell_set_hostname(const char* h);
void shell_set_cwd(const char* p);

#endif
