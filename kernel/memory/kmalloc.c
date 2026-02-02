#include <stdint.h>
#include <stddef.h>
#include <kernel/printk.h> // Printk desteği ekle

extern uint32_t end; 
// Başlangıç adresini tam olarak görebilmek için fonksiyon içinde kontrol edelim
uintptr_t placement_address = 0;

void* kmalloc_int(size_t sz, int align, uintptr_t *phys) {
    // İlk çalıştırmada end adresini al
    if (placement_address == 0) {
        placement_address = (uintptr_t)&end;
        printk("KMALLOC Basladi: 0x%x\n", placement_address);
    }

    if (align == 1 && (placement_address & 0xFFF)) {
        placement_address &= 0xFFFFF000;
        placement_address += 0x1000;
    }
    
    if (phys) {
        *phys = placement_address;
    }

    uintptr_t tmp = placement_address;
    placement_address += sz;

    // Kritik: Her bellek ayrıldığında konsola bas ki 0x0 hatasını yakalayalım
    // printk("KMALLOC: %d bytes @ 0x%x\n", sz, tmp); 
    
    return (void*)tmp;
}

void* kmalloc(size_t sz) {
    return kmalloc_int(sz, 0, NULL);
}