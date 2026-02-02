#ifndef SERIAL_H
#define SERIAL_H

void serial_init(void);
void serial_putc(char c);
void serial_write(const char* str);
int  serial_received(void);  // Eklendi
char serial_getc(void);      // Eklendi

#endif