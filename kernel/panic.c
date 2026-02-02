#include <kernel/panic.h>
#include <kernel/printk.h>
#include <kernel/vga.h>
#include <kernel/serial.h>

void panic(const char *message) {
    // 1. Ekranı temizleyip kırmızı bir "Kernel Panic" uyarısı verelim
    vga_set_color(0x4); // Kırmızı renk (VGA_COLOR_RED)
    
    printk("\n------------------------------------------------\n");
    printk("KERNEL PANIC: %s\n", message);
    printk("Sistem durduruldu. Lutfen yeniden baslatin.\n");
    printk("------------------------------------------------\n");

    // 2. Aynı mesajı seri porta da gönder (QEMU logları için)
    serial_write("\n!!! KERNEL PANIC !!!\n");
    serial_write(message);
    serial_write("\nHALTING SYSTEM...\n");

    // 3. İşlemciyi sonsuz döngüye sok ve durdur
    // Interrupt'ları (kesmeleri) kapatıyoruz ki sistem uyanmasın
    asm volatile ("cli"); 
    while (1) {
        asm volatile ("hlt");
    }
}