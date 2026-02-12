#pragma once
#include <stdint.h>

void fb_console_init(uint32_t fg, uint32_t bg);
void fb_console_putc(char c);
void fb_console_write(const char* s);
void fb_console_clear(void);
void fb_console_flush(void);