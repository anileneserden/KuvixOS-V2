#include <kernel/printk.h>
#include <kernel/serial.h>
#include <kernel/vga.h>
#include <kernel/vga_font.h>
#include <lib/shell.h>
#include <kernel/fs.h>
#include <kernel/kmalloc.h> // kmalloc prototiplerini ekledik
#include <kernel/panic.h>

// Dışarıdan gelen fonksiyonlar
extern fs_node_t *init_ramfs(void);

void kernel_main(void) {
    // 1. Temel Donanım Başlatma
    serial_init();
    vga_init(); 
    vga_load_tr_font();

    // 2. KRİTİK ADIM: Bellek Yönetimi Başlatma
    // kmalloc'un 'end' sembolünü tanıması ve placement_address'i 
    // ayarlaması için önce bunu çağırmalıyız (eğer init fonksiyonun varsa).
    // Senin kmalloc.c'de ilk kullanımda set ediyor, bu yüzden direkt devam edebiliriz.
    
    printk("KuvixOS V2 Kernel Yuklendi.\n");

    // 3. Dosya Sistemi Başlatma (VFS & RamFS)
    // ÖNEMLİ: init_ramfs içinde kmalloc çağrıldığı için kmalloc hazır olmalı.
    fs_root = init_ramfs();
    
    if (fs_root != 0) {
        fs_current = fs_root;
        printk("VFS: RamFS '/' uzerine baglandi. KOK: 0x%x\n", (uintptr_t)fs_root);
    } else {
        // Eğer burası 0 gelirse sistem çöker, önlem alalım
        panic("VFS baslatilamadi! fs_root NULL.");
    }

    // 4. Shell Başlatma
    shell_init();

    // 5. Ana Döngü
    // shell_init içindeki sonsuz döngüden çıkılırsa diye burası emniyet kemeridir.
    while(1) {
        asm volatile ("hlt"); 
    }
}