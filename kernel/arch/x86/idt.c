#include <stdint.h>
#include <arch/x86/io.h>
#include <kernel/serial.h>

struct idt_entry {
    uint16_t base_lo;
    uint16_t sel;
    uint8_t always0;
    uint8_t flags;
    uint16_t base_hi;
} __attribute__((packed));

struct idt_ptr {
    uint16_t limit;
    uint32_t base;
} __attribute__((packed));

static struct idt_entry idt[256];
static struct idt_ptr idtp;

// Assembly'den gelen köprüler
extern void timer_handler_asm(void);
extern void mouse_handler_asm(void); // Bunu dışarıda bildirdik
extern void keyboard_handler_asm(void);
extern void dummy_handler_asm(void);

void idt_set_gate(uint8_t num, uint32_t base, uint16_t sel, uint8_t flags) {
    idt[num].base_lo = base & 0xFFFF;
    idt[num].base_hi = (base >> 16) & 0xFFFF;
    idt[num].sel = sel;
    idt[num].always0 = 0;
    idt[num].flags = flags;
}

void idt_init() {
    serial_write("[IDT] Kurulum basliyor...\n");

    // 1. PIC Remap
    outb(0x20, 0x11); outb(0xA0, 0x11);
    outb(0x21, 0x20); outb(0xA1, 0x28);
    outb(0x21, 0x04); outb(0xA1, 0x02);
    outb(0x21, 0x01); outb(0xA1, 0x01);

    // 2. IDT Pointer ayarla
    idtp.limit = (uint16_t)(sizeof(struct idt_entry) * 256) - 1;
    idtp.base = (uint32_t)&idt;
    
    // 3. Varsayılan dolgu (Dummy)
    for(int i = 0; i < 256; i++) {
        idt_set_gate(i, (uint32_t)dummy_handler_asm, 0x08, 0x8E);
    }

    // 4. Donanım Kesmelerini Bağla
    idt_set_gate(32, (uint32_t)timer_handler_asm, 0x08, 0x8E);    // IRQ0: Timer
    
    // BURAYI EKLE: Eğer keyboard_handler_asm tanımlıysa kullan, 
    // değilse dummy kalsın ama maske ile kapatacağız.
    idt_set_gate(33, (uint32_t)keyboard_handler_asm, 0x08, 0x8E); // IRQ1: Klavye
    
    idt_set_gate(44, (uint32_t)mouse_handler_asm, 0x08, 0x8E);    // IRQ12: Mouse

    // 5. Kesme Maskelerini Ayarla (En Önemli Kısım!)
    // 0x00 her şeyi açar. Biz sadece ihtiyacımız olanları açalım:
    // Master PIC: Bit 0 (Timer), Bit 1 (Klavye), Bit 2 (Slave PIC Cascade - ŞART!)
    // 11111000 binary = 0xF8 -> Sadece IRQ 0, 1 ve 2 açık
    outb(0x21, 0xF8); 

    // Slave PIC: Bit 4 (Mouse IRQ12) açık olmalı.
    // IRQ 12, Slave PIC'in 4. bitidir (12-8=4).
    // 11101111 binary = 0xEF -> Sadece IRQ 12 açık
    outb(0xA1, 0xEF); 

    asm volatile("lidt %0" : : "m"(idtp));
}