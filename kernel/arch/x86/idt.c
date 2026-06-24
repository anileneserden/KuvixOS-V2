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

static void (*irq_handlers[16])(void) = {0};

void idt_register_irq_handler(int irq, void (*handler)(void)) {
    if (irq >= 0 && irq < 16) {
        irq_handlers[irq] = handler;
    }
}

extern void timer_handler_asm(void);
extern void mouse_handler_asm(void); 
extern void keyboard_handler_asm(void);
extern void dummy_handler_asm(void);

void mouse_handler_c(void) {
    if (irq_handlers[12]) {
        irq_handlers[12]();
    }

    outb(0xA0, 0x20);
    outb(0x20, 0x20);
}

void idt_set_gate(uint8_t num, uint32_t base, uint16_t sel, uint8_t flags) {
    idt[num].base_lo = base & 0xFFFF;
    idt[num].base_hi = (base >> 16) & 0xFFFF;
    idt[num].sel = sel;
    idt[num].always0 = 0;
    idt[num].flags = flags;
}

void idt_init() {
    serial_write("[IDT] Kurulum basliyor...\n");

    outb(0x20, 0x11); outb(0xA0, 0x11);
    outb(0x21, 0x20); outb(0xA1, 0x28);
    outb(0x21, 0x04); outb(0xA1, 0x02);
    outb(0x21, 0x01); outb(0xA1, 0x01);

    idtp.limit = (uint16_t)(sizeof(struct idt_entry) * 256) - 1;
    idtp.base = (uint32_t)&idt;
    
    for(int i = 0; i < 256; i++) {
        idt_set_gate(i, (uint32_t)dummy_handler_asm, 0x08, 0x8E);
    }

    idt_set_gate(32, (uint32_t)timer_handler_asm, 0x08, 0x8E);
    idt_set_gate(33, (uint32_t)keyboard_handler_asm, 0x08, 0x8E);
    idt_set_gate(44, (uint32_t)mouse_handler_asm, 0x08, 0x8E);

    outb(0x21, 0xF8);
    outb(0xA1, 0xEF);

    asm volatile("lidt %0" : : "m"(idtp));
}