#ifndef SHELL_H
#define SHELL_H

void shell_init(void);
void shell_readline(char* buffer, int max_len);
void shell_set_username(const char* u);
void shell_set_hostname(const char* h);
void shell_set_cwd(const char* p);
const char* shell_get_cwd(void);

#endif