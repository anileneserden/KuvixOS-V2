#include <kernel/serial.h>
#include <arch/x86/io.h>

#define COM1 0x3F8

void serial_init(void) {
    outb(COM1 + 1, 0x00);    // Kesmeleri devre dışı bırak
    outb(COM1 + 3, 0x80);    // DLAB kilidini aç
    outb(COM1 + 0, 0x01);    // Baud Rate: 115200 (0x01)
    outb(COM1 + 1, 0x00);
    outb(COM1 + 3, 0x03);    // 8 bit, parite yok, 1 stop biti
    outb(COM1 + 2, 0xC7);    // FIFO etkin
    outb(COM1 + 4, 0x0B);    // IRQs etkin
}

int serial_is_transmit_empty(void) {
    return inb(COM1 + 5) & 0x20;
}

void serial_putc(char c) {
    while (!serial_is_transmit_empty());
    outb(COM1, c);
}

void serial_write(const char* str) {
    while (*str) {
        if (*str == '\n') serial_putc('\r');
        serial_putc(*str++);
    }
}

int serial_received(void) {
    return inb(COM1 + 5) & 1;
}

char serial_getc(void) {
    while (serial_received() == 0);
    return inb(COM1);
}