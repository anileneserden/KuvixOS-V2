#include <stdint.h>
#include <arch/x86/io.h>
#include <kernel/serial.h>
#include <ui/notification.h> // notification_show için şart

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
extern void mouse_handler_asm(void);
extern void keyboard_handler_asm(void);
extern void dummy_handler_asm(void);
extern void syscall_handler_asm(void); // Assembly'deki yeni fonksiyon

// C Tarafındaki Syscall İşleyicisi
void handle_syscall(uint32_t eax, uint32_t ebx, uint32_t ecx) {
    switch(eax) {
        case 100: // KEF-v3 Notification ID
            // ebx: Mesajın adresi, ecx: Gösterim süresi
            notification_show((const char*)ebx, ecx);
            break;
            
        default:
            // Bilinmeyen syscall durumunda debug mesajı basabilirsin
            serial_write("[Syscall] Bilinmeyen cagri!\n");
            break;
    }
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

    // 4. Donanım Kesmelerini Bağla (IRQ)
    idt_set_gate(32, (uint32_t)timer_handler_asm, 0x08, 0x8E);    
    idt_set_gate(33, (uint32_t)keyboard_handler_asm, 0x08, 0x8E); 
    idt_set_gate(44, (uint32_t)mouse_handler_asm, 0x08, 0x8E);    

    // 5. Yazılım Kesmesi (Syscall - int 0x80)
    // 0xEE: Mevcut (1), Ring 3 (11), 32-bit Interrupt Gate (01110)
    // Bu sayede kullanıcı uygulamaları bu kesmeyi tetikleyebilir.
    idt_set_gate(0x80, (uint32_t)syscall_handler_asm, 0x08, 0xEE);

    // 6. Kesme Maskelerini Ayarla
    outb(0x21, 0xF8); // IRQ 0, 1, 2 açık
    outb(0xA1, 0xEF); // IRQ 12 açık

    asm volatile("lidt %0" : : "m"(idtp));
    serial_write("[IDT] Kurulum tamamlandi, Syscall aktif.\n");
}