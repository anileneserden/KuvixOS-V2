#include <kernel/power.h>
#include <arch/x86/io.h>
#include <kernel/panic.h>
#include <stdint.h>

// İşlemciyi tamamen durdurma (Kritik hatalar için)
void power_halt(void)
{
    __asm__ __volatile__("cli");
    for (;;) {
        __asm__ __volatile__("hlt");
    }
}

// Sistemi Kapatma
power_result_t power_shutdown(void)
{
    __asm__ __volatile__("cli");

    // Not: panic() fonksiyonun içinde power_reboot çağrısı varsa 
    // burası reboot tetikleyebilir. Sadece mesaj basmak istersen printk kullanabilirsin.
    // panic("SISTEM KAPATILIYOR..."); 

    // QEMU/Bochs Kapatma Komutları
    outw(0x604, 0x2000);   // QEMU/Bochs modern
    outw(0xB004, 0x2000);  // Bochs eski
    outw(0x4004, 0x3400);  // VirtualBox/Alternatif QEMU

    power_halt();
    return POWER_OK; 
}

// 1. Yöntem: PS/2 Keyboard Controller Reset (Klasik)
static void reboot_8042(void)
{
    for (int i = 0; i < 100; i++) {
        // Status register (0x64) Bit 1: Input buffer dolu mu?
        if ((inb(0x64) & 0x02) == 0) {
            outb(0x64, 0xFE); // Reset komutu
        }
        // Kısa bir gecikme
        for(volatile int j = 0; j < 1000; j++);
    }
}

// 2. Yöntem: PCI Reset Register (Modern)
static void reboot_pci(void)
{
    // 0xCF9 portu Reset Control Register'dır. 
    // 0x06 (Bit 1 ve 2) 'Hard Reset' sinyali gönderir.
    outb(0xCF9, 0x06);
}

// 3. Yöntem: Triple Fault (Acil Durum / Fallback)
__attribute__((noreturn)) static void reboot_triple_fault(void)
{
    struct __attribute__((packed)) {
        uint16_t limit;
        uint32_t base;
    } idtr = { 0, 0 };

    __asm__ __volatile__("lidt %0" : : "m"(idtr));
    __asm__ __volatile__("int $0x03"); // Geçersiz IDT ile interrupt tetikle -> Triple Fault!
    
    for (;;) { __asm__ __volatile__("hlt"); }
}

// Ana Reboot Fonksiyonu
power_result_t power_reboot(void)
{
    __asm__ __volatile__("cli");

    // En etkili yöntemden en kaba yönteme doğru deniyoruz:
    
    // 1. PCI Reset denemesi
    reboot_pci();

    // 2. Klavye Kontrolcüsü denemesi
    reboot_8042();

    // 3. Triple Fault (Eğer donanım reset sinyallerini yuttuysa CPU'yu patlatır)
    reboot_triple_fault();

    return POWER_OK; // Buraya asla ulaşılmamalı
}