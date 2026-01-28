#include <kernel/printk.h>
#include <kernel/serial.h>

void kernel_main(void) {
    serial_init();

    printk("KuvixOS V2\n");
    printk("Serial + VGA OK\n");

    for (;;) {
        asm volatile ("hlt");
    }
}
