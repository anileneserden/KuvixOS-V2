#include <stdint.h>
#include <arch/x86/io.h>

#define COM1 0x3F8

static int serial_is_transmit_empty(void) {
    return inb(COM1 + 5) & 0x20;
}

void serial_putc(char c) {
    while (!serial_is_transmit_empty());
    outb(COM1, c);
}

void serial_write(const char* str) {
    while (*str) {
        if (*str == '\n')
            serial_putc('\r');
        serial_putc(*str++);
    }
}

void serial_init(void) {
    outb(COM1 + 1, 0x00);    // Disable interrupts
    outb(COM1 + 3, 0x80);    // Enable DLAB
    outb(COM1 + 0, 0x03);    // Baud rate (38400)
    outb(COM1 + 1, 0x00);
    outb(COM1 + 3, 0x03);    // 8 bits, no parity
    outb(COM1 + 2, 0xC7);    // FIFO
    outb(COM1 + 4, 0x0B);    // IRQs enabled
}
