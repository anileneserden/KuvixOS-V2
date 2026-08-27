#include <lib/sha256.h>
#include <lib/commands.h>
#include <lib/string.h>
#include <kernel/printk.h>

void cmd_sha256sum(int argc, char** argv) {
    if (argc < 2) {
        printk("Kullanim: sha256sum <metin>\n");
        printk("Ornek: sha256sum merhaba\n");
        return;
    }

    const char* input = argv[1];
    SHA256_CTX ctx;
    uint8_t hash[32];

    // SHA-256 hesaplama sürecini başlat ve çalıştır
    sha256_init(&ctx);
    sha256_update(&ctx, (const uint8_t*)input, strlen(input));
    sha256_final(&ctx, hash);

    // Çıkan 32 baytlık ham veriyi 16'lık tabanda (hex) ekrana yazdıralım
    printk("SHA256 (%s) = ", input);
    for (int i = 0; i < 32; i++) {
        printk("%x2", hash[i]); // İki basamaklı hex formatı
    }
    printk("\n");
}

REGISTER_COMMAND(sha256sum, cmd_sha256sum, "Girilen metnin SHA-256 hash özetini hesaplar");