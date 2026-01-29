#include <kernel/printk.h>
#include <kernel/serial.h>
#include <kernel/vga.h>
#include <kernel/vga_font.h>
#include <lib/shell.h>

void kernel_main(void) {
    serial_init();
    vga_init(); 

    // Fontları shell'den ÖNCE yükle
    vga_load_tr_font();

    // Manuel test: Eğer font yüklendiyse ekranda "ğ ş ı" harflerini görmelisin
    printk("KuvixOS Turkce Karakter Destegi:\n");
    printk("Kucuk: ğ ş ı ö ç ü\n");
    printk("Buyuk: Ğ Ş İ Ö Ç Ü\n");
    printk("Cümle: Çiğ süt emmiş şoförler ılık su içer.\n");
    
    printk("KuvixOS V2 Kernel Yüklendi.\n");
    shell_init(); 

    for (;;) { asm volatile ("hlt"); }
}