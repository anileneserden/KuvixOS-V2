#include <kernel/printk.h>
#include <kernel/serial.h>
#include <kernel/vga.h>
#include <kernel/vga_font.h>
#include <lib/shell.h>
#include <kernel/fs.h>

// ramfs.c içinde tanımladığımız başlatıcı fonksiyonu dışarıdan alıyoruz
extern fs_node_t *init_ramfs(void);

void kernel_main(void) {
    // 1. Temel Donanım Başlatma
    serial_init();
    vga_init(); 

    // 2. Türkçe Karakter Desteği (Ekran)
    vga_load_tr_font();

    // 3. Dosya Sistemi Başlatma (VFS & RamFS)
    // Önce VFS'in kök dizinini RamFS ile dolduruyoruz
    fs_root = init_ramfs();
    
    // Bilgilendirme mesajları
    printk("KuvixOS V2 Kernel Yuklendi.\n");
    if (fs_root != 0) {
        printk("VFS: RamFS '/' uzerine baglandi.\n");
    }

    // 4. Shell Başlatma
    // shell_init muhtemelen klavye sürücüsünü hazırlar
    shell_init();

    // 5. Ana Döngü (Shell Girişi)
    printk("KuvixOS Komut Satiri Hazir.\n\n");
    
    // ÖNEMLİ: Shell'in sürekli çalışması için bir döngü gerekir.
    // Eğer shell_loop gibi bir fonksiyonun varsa onu burada çağır.
    // Yoksa, shell_init içinde bir döngü olduğundan emin olmalısın.
    
    // Eğer shell bir kez çalışıp bitiyorsa:
    while(1) {
        // shell_update(); // Varsa klavye tarama fonksiyonun
        asm volatile ("hlt"); 
    }
}