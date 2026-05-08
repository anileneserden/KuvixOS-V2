#ifndef PRINTK_H
#define PRINTK_H

#include <stdarg.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

void printk(const char* fmt, ...);
int ksprintf(char *buf, const char *fmt, ...);
void printk_set_gui_mode(bool enable);

#ifdef __cplusplus
}
#endif

#endif