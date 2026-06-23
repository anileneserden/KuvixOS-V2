#include <lib/commands.h>
#include <kernel/printk.h>
#include <lib/string.h>
#include <kernel/kdf.h>

void cmd_driver(int argc, char** argv) {
    if (argc < 2) {
        commands_puts("Kullanim:\n");
        commands_puts("  driver load <dosya_yolu>   -> Belirtilen .kdf surucusunu yukler\n");
        commands_puts("  driver list                -> Yuklu tum dinamik suruculeri listeler\n");
        return;
    }

    if (strcmp(argv[1], "load") == 0) {
        if (argc < 3) {
            commands_puts("Hata: Lutfen yuklenecek surucu yolunu belirtin!\n");
            return;
        }
        
        int ret = kdf_load_driver(argv[2]);
        if (ret == 0) {
            printk("[DRIVER] Surucu basariyla yuklendi: %s\n", argv[2]);
        } else {
            printk("[DRIVER] Surucu yukleme basarisiz! Hata Kodu: %d\n", ret);
        }
    } 
    else if (strcmp(argv[1], "list") == 0) {
        // Çekirdekteki genişletilmiş tablo fonksiyonunu çağırıyoruz
        kdf_list_drivers(); 
    } 
    else {
        commands_puts("Bilinmeyen alt komut. 'load' veya 'list' kullanin.\n");
    }
}

REGISTER_COMMAND(driver, cmd_driver, "Surucu yonetimi: driver load <yol> veya driver list");