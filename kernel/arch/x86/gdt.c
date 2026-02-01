#include <stdint.h>

// GDT Giriş Yapısı
struct gdt_entry {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t  base_middle;
    uint8_t  access;
    uint8_t  granularity;
    uint8_t  base_high;
} __attribute__((packed));

// GDT Pointer Yapısı (İşlemciye verilecek olan)
struct gdt_ptr {
    uint16_t limit;
    uint32_t base;
} __attribute__((packed));

struct gdt_entry gdt[3];
struct gdt_ptr gp;

// GDT'yi işlemciye yüklemek için basit bir fonksiyon
void gdt_init() {
    gp.limit = (sizeof(struct gdt_entry) * 3) - 1;
    gp.base  = (uint32_t)&gdt;

    // 1. Null
    gdt[0] = (struct gdt_entry){0, 0, 0, 0, 0, 0};
    // 2. Code (0x08)
    gdt[1] = (struct gdt_entry){0xFFFF, 0, 0, 0x9A, 0xCF, 0};
    // 3. Data (0x10)
    gdt[2] = (struct gdt_entry){0xFFFF, 0, 0, 0x92, 0xCF, 0};

    asm volatile(
        "lgdt %0\n\t"              // GDT'yi yükle
        "mov $0x10, %%ax\n\t"      // 0x10: Data Segment indeksi
        "mov %%ax, %%ds\n\t"
        "mov %%ax, %%es\n\t"
        "mov %%ax, %%ss\n\t"
        "ljmp $0x08, $.next\n\t"   // 0x08: Code Segment'e Far Jump (CS'yi günceller)
        ".next:\n\t"
        : : "m"(gp) : "eax"
    );
}