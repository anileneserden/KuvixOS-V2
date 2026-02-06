#include <stdint.h>
#include <multiboot2.h>
#include <kernel/drivers/video/fb.h>
#include <kernel/drivers/video/gfx.h>
#include <kernel/serial.h>
#include <ui/desktop.h>
#include <ui/theme.h>
#include <kernel/printk.h>
#include <arch/x86/gdt.h>
#include <arch/x86/idt.h>
#include <kernel/time.h>
#include <kernel/drivers/input/mouse_ps2.h>
#include <kernel/drivers/ata_pio.h>
#include <kernel/block/block.h>
#include <ui/messagebox.h>
#include <lib/string.h>
#include <lib/shell.h>

// Dış fonksiyon bildirimleri
extern void gdt_init(void);
extern void idt_init(void);
extern void time_init_from_rtc(void);
extern void timer_init(uint32_t freq);
extern void ps2_mouse_init(void);
extern void ui_installer_retro_run(void); 

void kernel_main(uint32_t magic, multiboot_info_t* mbi) {
    serial_init();
    gdt_init();      
    idt_init();

    // 1. Görüntü Birimini Hazırla
    if (magic == 0x2BADB002 && (mbi->flags & (1 << 12))) {
        fb_init((uint32_t)mbi->framebuffer_addr, (uint32_t)mbi->framebuffer_width,
                (uint32_t)mbi->framebuffer_height, (uint32_t)mbi->framebuffer_pitch); 
    } else {
        fb_init(0xFD000000, 1024, 768, 1024 * 4); 
    }

    gfx_init();
    ui_theme_bootstrap_default();
    gfx_clear(0x1a1a1a);
    printk("KuvixOS v2 Baslatiliyor...\n");
    fb_present(); 

    // 2. Zamanlayıcı ve Donanım
    time_init_from_rtc();
    timer_init(1000);

    // 3. Akıllı Disk Taraması
    bool installation_found = false;
    if (ata_pio_init()) {
        int disk_count = ata_pio_get_disk_count();
        char boot_buf[512];

        for (int i = 0; i < disk_count; i++) {
            ata_disk_t* d = ata_pio_get_disk(i);
            // Sadece loglamak için printk, fb_present her adımda gerekmez ama debug için iyi
            if (ata_pio_read_disk(d, 0, boot_buf)) {
                if (strncmp(boot_buf, "KUVIX_OS", 8) == 0) {
                    installation_found = true;
                    // Eğer kurulum bulunduysa bu diski kök aygıt yap
                    block_set_root(ata_pio_get_dev()); 
                    break;
                }
            }
        }
    }

    // 4. Kesmeler ve Input
    asm volatile("sti"); 
    
    // Hataya meyilli olduğu için printk ekledik
    printk("Giris birimleri yukleniyor...\n");
    fb_present();
    ps2_mouse_init(); 

    // 5. GRUB'dan gelen komut satırı parametrelerini kontrol et (Hala destekleyelim)
    bool force_install = false;
    if (mbi->flags & (1 << 0)) { 
        char* cmdline = (char*)mbi->cmdline;
        if (cmdline && strstr(cmdline, "--install") != NULL) {
            force_install = true;
        }
    }

    // 6. NİHAİ KARAR
    if (force_install) {
        printk("MOD: Zorunlu Kurulum\n");
        fb_present();
        ui_installer_retro_run();
    } 
    else if (installation_found) {
        printk("MOD: Kurulu Sistem (Diskten)\n");
        fb_present();
        ui_desktop_run(); // Doğrudan masaüstü!
    } 
    else {
        printk("MOD: Canli (Kurulum Bulunamadi)\n");
        fb_present();
        ui_installer_retro_run(); // Kurulum yoksa kullanıcıya kurdur
    }

    while(1) { asm volatile("hlt"); }
}