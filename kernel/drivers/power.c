#include <kernel/power.h>
#include <arch/x86/io.h>

void power_halt(void)
{
    __asm__ __volatile__("cli");
    for (;;) {
        __asm__ __volatile__("hlt");
    }
}

power_result_t power_shutdown(void)
{
    __asm__ __volatile__("cli");

    // QEMU (isa-debug-exit / ACPI poweroff port) - çoğu setup'ta çalışır
    // QEMU: -device isa-debug-exit,iobase=0xf4,iosize=0x04  kullanıyorsan 0xF4 de var
    // Ama senin loglardan genelde ACPI portları yeterli oluyor.

    // ACPI PM1a control (bazı emülatörlerde)
    outw(0x604, 0x2000);   // QEMU/Bochs sık kullanılan
    outw(0xB004, 0x2000);  // Bochs eski yöntem

    // Bazı QEMU konfiglerinde işe yarayan alternatif
    outw(0x4004, 0x3400);

    // Eğer kapanmadıysa CPU'yu durdur
    power_halt();
    return POWER_OK; // buraya normalde gelmez
}

static void reboot_8042(void)
{
    // 8042 keyboard controller reset
    // Input buffer boşalana kadar bekle
    for (int i = 0; i < 100000; i++) {
        if ((inb(0x64) & 0x02) == 0) break;
    }
    outb(0x64, 0xFE);
}

__attribute__((noreturn)) static void reboot_triple_fault(void)
{
    // IDT'yi 0 yapıp interrupt tetikleyerek triple fault ile reset
    struct __attribute__((packed)) {
        uint16_t limit;
        uint32_t base;
    } idtr = { 0, 0 };

    __asm__ __volatile__("lidt %0" : : "m"(idtr));
    __asm__ __volatile__("int $0x03"); // triple fault tetiklemesi için
    for (;;) { __asm__ __volatile__("hlt"); }
}

power_result_t power_reboot(void)
{
    __asm__ __volatile__("cli");

    reboot_8042();

    // Eğer reset olmadıysa triple fault fallback
    reboot_triple_fault();
}
