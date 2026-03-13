#include <lib/commands.h>
#include <kernel/power.h>
#include <kernel/drivers/rtc/rtc.h>
#include <lib/string.h>
#include <kernel/printk.h>

// Shell'in erişebileceği hedef zaman değişkenleri (shell.c'de tanımlayacağız)
extern int g_shutdown_target_hour;
extern int g_shutdown_target_min;

void cmd_shutdown(int argc, char** argv)
{
    // 1. --now parametresi varsa hiç bekleme
    if (argc > 1 && strcmp(argv[1], "--now") == 0) {
        printk("Sistem aninda kapatiliyor...\n");
        power_shutdown();
        while(1);
    }

    // 2. Normal kapatma: RTC'den saati oku
    rtc_datetime_t now;
    if (!rtc_read_datetime(&now)) {
        printk("Hata: RTC okunamazsa sistem aninda kapatiliyor...\n");
        power_shutdown();
        return;
    }

    // Hedef zamanı hesapla (+1 dakika)
    int target_m = now.min + 1;
    int target_h = now.hour;

    if (target_m >= 60) {
        target_m = 0;
        target_h = (target_h + 1) % 24;
    }

    // Shell'deki takip değişkenlerini set et
    g_shutdown_target_hour = target_h;
    g_shutdown_target_min  = target_m;

    printk("\n[!] Kapatma islemi zamanlandi.\n");
    printk("[!] Mevcut Saat: %02d:%02d\n", now.hour, now.min);
    printk("[!] Kapatma Saati: %02d:%02d (1 dakika sonra)\n", target_h, target_m);
    printk("[!] Kapatilana kadar calismaya devam edebilirsiniz.\n");
}

REGISTER_COMMAND(shutdown, cmd_shutdown, "Sistemi kapatir. Kullanim: shutdown [--now]");