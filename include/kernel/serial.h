#pragma once
#include <stdint.h>

void serial_init(void);
void serial_putc(char c);
void serial_write(const char* str);
int  serial_received(void);
char serial_getc(void);

void serial_write_hex8(uint8_t x);
void serial_write_u16_hex(uint16_t x);