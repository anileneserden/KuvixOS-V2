#include <kernel/printk.h>
#include <kernel/serial.h>
#include <kernel/vga.h>
#include <shell.h>

void kernel_main(void) {
    serial_init();
    vga_init(); // VGA ekranını temizlemek için (imleci 0,0 yapar)

    printk("KuvixOS V2 Kernel Yuklendi.\n");

    // Shell'i başlatıyoruz. shell_init içindeki while(1) 
    // sayesinde sistem burada kalacaktır.
    shell_init(); 

    // Eğer shell'den bir şekilde çıkılırsa sistemi durdur.
    for (;;) {
        asm volatile ("hlt");
    }
}