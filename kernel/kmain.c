#include <kernel/printk.h>
#include <kernel/serial.h>
#include <kernel/vga.h>
#include <kernel/fs/vfs.h>
#include <kernel/fs/fs_init.h>
#include <lib/shell.h>
#include <kernel/drivers/video/fb.h>
#include <kernel/drivers/video/gfx.h>
#include <ui/desktop.h>
#include <ui/theme.h>

void kernel_main(void) {
    serial_init();
    vga_init(); // İlk aşamada metin mesajları için
    
    printk("KuvixOS V2 Baslatiliyor...\n");

    // 1. Dosya sistemi ve Disk ilklendirme
    if (fs_init_once()) {
        printk("Dosya sistemi ve Diskler hazir.\n");
    } else {
        printk("HATA: Dosya sistemi baslatilamadi!\n");
    }

    // 2. Grafik Modu Hazırlığı
    // Not: 0xFD000000 genel bir LFB adresidir, QEMU/BGA için uygundur.
    fb_init(0xFD000000); 
    gfx_init();

    printk("Masaustu yukleniyor...\n");

    // Önce tema motorunu hazırla
    ui_theme_bootstrap_default();

    // Sonra masaüstünü çalıştır
    ui_desktop_run();

    // Eğer masaüstünden çıkılırsa (teorik olarak), Shell'e düşebilirsin.
    shell_init();

    while(1) { 
        asm volatile("hlt"); 
    }
}