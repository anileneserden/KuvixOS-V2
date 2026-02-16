#include <kernel/serial.h>
#include <arch/x86/io.h>
#include <stdint.h>

#define COM1 0x3F8

void serial_init(void) {
    outb(COM1 + 1, 0x00);    // Interrupt disable
    outb(COM1 + 3, 0x80);    // DLAB on
    outb(COM1 + 0, 0x01);    // Divisor low  (115200 baud)
    outb(COM1 + 1, 0x00);    // Divisor high
    outb(COM1 + 3, 0x03);    // 8N1
    outb(COM1 + 2, 0xC7);    // FIFO enable, clear, 14-byte threshold
    outb(COM1 + 4, 0x0B);    // IRQs enabled, RTS/DSR set (debug için şart değil ama kalsın)
}

int serial_is_transmit_empty(void) {
    return inb(COM1 + 5) & 0x20;
}

void serial_putc(char c) {
    // ✅ Debug güvenliği: COM1 yoksa kernel'i kilitlemesin diye timeout
    for (int i = 0; i < 200000; i++) {
        if (serial_is_transmit_empty()) {
            outb(COM1, c);
            return;
        }
    }
    // timeout -> sessizce düş
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
    while (serial_received() == 0) { }
    return inb(COM1);
}

// --------------------
// ✅ HEX HELPERS
// --------------------
static char hex_digit(uint8_t v) { return (v < 10) ? ('0' + v) : ('A' + (v - 10)); }

void serial_write_hex8(uint8_t x) {
    char s[3];
    s[0] = hex_digit((x >> 4) & 0xF);
    s[1] = hex_digit(x & 0xF);
    s[2] = 0;
    serial_write(s);
}

void serial_write_u16_hex(uint16_t x) {
    serial_write_hex8((uint8_t)((x >> 8) & 0xFF));
    serial_write_hex8((uint8_t)(x & 0xFF));
}