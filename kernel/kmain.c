#include <kernel/printk.h>
#include <kernel/serial.h>
#include <kernel/vga.h>
#include <kernel/fs/vfs.h>
#include <kernel/fs/fs_init.h>
#include <lib/shell.h>

void kernel_main(void) {
    serial_init();
    vga_init();
    
    printk("KuvixOS V2 Baslatiliyor...\n");

    // Bellek ve VFS/Disk ilklendirme
    if (fs_init_once()) {
        printk("Dosya sistemi ve Diskler hazir.\n");
    } else {
        printk("HATA: Dosya sistemi baslatilamadi!\n");
    }

    shell_init();

    while(1) { asm volatile("hlt"); }
}