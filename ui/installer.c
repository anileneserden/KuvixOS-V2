#include <kernel/drivers/video/gfx.h>
#include <kernel/drivers/video/fb.h>
#include <kernel/drivers/input/keyboard.h>
#include <kernel/drivers/ata_pio.h>
#include <kernel/power.h>
#include <lib/string.h>
#include <ui/messagebox.h>
#include <kernel/printk.h>

// Kurulum işlemini yapan yardımcı fonksiyon
bool perform_installation(ata_disk_t* dest_disk) { // void yerine bool yaptık
    if (!dest_disk) return false; // Güvenlik kontrolü

    char boot_sector[512];
    memset(boot_sector, 0, 512);
    memcpy(boot_sector, "KUVIX_OS", 8);
    boot_sector[510] = (char)0x55;
    boot_sector[511] = (char)0xAA;

    // Yazma işleminin sonucunu döndür
    return ata_pio_write_disk(dest_disk, 0, boot_sector);
}

void ui_installer_retro_run() {
    int stage = 0; 
    int selected_btn = 0;   
    int selected_disk = 0;  
    bool done = false;

    ata_pio_scan_all(); 

    while(!done) {
        gfx_clear(0x0000AA);
        int ww = 580, wh = 340;
        int wx = (fb_get_width() - ww) / 2;
        int wy = (fb_get_height() - wh) / 2;

        // --- PENCERE ÇİZİMİ ---
        gfx_fill_rect(wx + 10, wy + 10, ww, wh, 0x000000); 
        gfx_fill_rect(wx, wy, ww, wh, 0xAAAAAA);          
        gfx_draw_rect(wx, wy, ww, wh, 0xFFFFFF);          
        gfx_fill_rect(wx, wy, ww, 24, 0x000088);
        gfx_draw_text(wx + 10, wy + 5, 0xFFFFFF, "KuvixOS Kurulum Yoneticisi");

        if (stage == 0) {
            /* KARŞILAMA */
            gfx_draw_text(wx + 20, wy + 50, 0x000000, "KuvixOS Kurulumuna Hosgeldiniz.");
            gfx_draw_text(wx + 20, wy + 80, 0x000000, "Sistem dosyalarini kalici diskinize kurmak");
            gfx_draw_text(wx + 20, wy + 100, 0x000000, "uzeresiniz.");
            gfx_draw_text(wx + 20, wy + 140, 0x000000, "Devam etmek icin ENTER'a basin.");
            
            uint32_t c = 0x0000AA;
            gfx_fill_rect(wx + 40, wy + 200, 500, 32, c);
            gfx_draw_text(wx + 210, wy + 208, 0xFFFFFF, "DEVAM ET");
        } 
        
        else if (stage == 1) {
            /* DİSK SEÇİMİ */
            gfx_draw_text(wx + 20, wy + 40, 0x000000, "Lutfen hedef diski secin:");

            int box_y = wy + 60;
            gfx_fill_rect(wx + 20, box_y, ww - 40, 140, 0x000000);
            gfx_draw_rect(wx + 20, box_y, ww - 40, 140, 0x555555);

            int count = ata_pio_get_disk_count();
            if (count == 0) {
                gfx_draw_text(wx + 35, box_y + 60, 0xFF0000, "HATA: Hicbir disk algilanmadi!");
            } else {
                for (int i = 0; i < count; i++) {
                    ata_disk_t* d = ata_pio_get_disk(i);
                    uint32_t color = (selected_disk == i) ? 0xFFFF00 : 0xFFFFFF;
                    
                    if (selected_disk == i) {
                        gfx_fill_rect(wx + 25, box_y + 10 + (i * 25), ww - 50, 20, 0x222222);
                    }

                    char info[128];
                    ksprintf(info, "[%c] Disk %d: %s - %s", 
                        (selected_disk == i ? '>' : ' '), i, d->model, 
                        (d->drive == 0xA0 ? "Master" : "Slave"));
                    
                    gfx_draw_text(wx + 35, box_y + 12 + (i * 25), color, info);
                }
            }

            gfx_draw_text(wx + 20, wy + 210, 0x333333, "YON TUSLARI: Sec | R: Yeniden Tara");

            uint32_t btn_c = 0x0000AA;
            gfx_fill_rect(wx + 40, wy + 250, 500, 32, btn_c);
            gfx_draw_text(wx + 150, wy + 258, 0xFFFFFF, "SECILI DISKE KURULUM YAP");
        }

        else if (stage == 2) {
            /* YÜKLEME AŞAMASI */
            gfx_draw_text(wx + 20, wy + 60, 0x000000, "Kurulum baslatildi...");
            fb_present(); 

            // HATA BURADAYDI: selected_disk (int) yerine diskin pointer'ını alıyoruz
            ata_disk_t* target = ata_pio_get_disk(selected_disk);

            if (perform_installation(target)) {
                stage = 3; 
            } else {
                gfx_draw_text(wx + 20, wy + 150, 0xAA0000, "HATA: Yazma islemi basarisiz!");
                fb_present();
            }
        }

        else if (stage == 3) {
            /* BİTTİ EKRANI */
            gfx_draw_text(wx + 20, wy + 60, 0x000000, "TEBRIKLER!");
            gfx_draw_text(wx + 20, wy + 90, 0x000000, "KuvixOS basariyla kuruldu.");
            gfx_draw_text(wx + 20, wy + 130, 0x000000, "Sistemi yeniden baslatmak icin ENTER'a basin.");

            gfx_fill_rect(wx + 40, wy + 250, 500, 32, 0x008800);
            gfx_draw_text(wx + 180, wy + 258, 0xFFFFFF, "REBOOT (YENIDEN BASLAT)");
        }
        
        fb_present();

        // --- KLAVYE KONTROLÜ ---
        uint16_t sc = kbd_pop_event();
        if (sc == 0) continue;

        if (sc == 0x48) { // YUKARI
            if (stage == 1 && selected_disk > 0) selected_disk--;
        }
        if (sc == 0x50) { // AŞAĞI
            if (stage == 1 && selected_disk < ata_pio_get_disk_count() - 1) selected_disk++;
        }
        if (sc == 0x13) { // 'R' Tuşu
            ata_pio_scan_all();
        }
        if (sc == 0x01) { // ESC (Hata durumunda geri dönmek için)
            if (stage == 2) stage = 1;
        }
        if (sc == 0x1C) { // ENTER
            if (stage == 0) stage = 1;
            else if (stage == 1) {
                if (ata_pio_get_disk_count() > 0) stage = 2;
            }
            else if (stage == 3) {
                // Yeniden başlat
                power_reboot(); // Bu fonksiyon kernel/power.h içinde tanımlı olmalı
            }
        }
    }
}