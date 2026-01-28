#include <kernel/printk.h>
#include <kernel/vga.h>
#include <kernel/serial.h>

void printk(const char* str) {
    vga_print(str);
    serial_write(str);
}
