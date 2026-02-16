#pragma once
#include <stdint.h>

void fb_console_init(uint32_t fg, uint32_t bg);
void fb_console_putc(char c);
void fb_console_write(const char* s);
void fb_console_write_utf8(const char* s);
void fb_console_clear(void);
void fb_console_flush(void);

void fb_console_set_cursor(int col, int row);
void fb_console_get_cursor(int* out_col, int* out_row);

int fb_console_cols(void);
int fb_console_rows(void);