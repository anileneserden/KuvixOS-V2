#include <kernel/printk.h>
#include <kernel/drivers/net/net.h>
#include <kernel/fs/kvxfs.h> 
#include <lib/commands.h>
#include <lib/string.h>
#include <stdint.h>

static void cmd_wget(int argc, char** argv) {
    if (argc < 2) {
        commands_puts("Kullanim: wget <dosya_adi>\n");
        return;
    }

    char* filename = argv[1];
    uint32_t server_ip = 0;
    char* host = "kuvixos.com.tr";
    
    char full_path[128];
    memset(full_path, 0, 128);
    strcpy(full_path, "/files/");
    strcat(full_path, filename);

    commands_puts("Sunucuya baglaniliyor: "); commands_puts(host); commands_puts("\n");

    // 1. DNS Çözümleme
    if (!net_dns_resolve_a(host, 0x08080808, &server_ip)) {
        commands_puts("Hata: Host bulunamadi (DNS).\n");
        return;
    }

    printk("[DEBUG] Sunucu IP: %d.%d.%d.%d\n", 
           (server_ip >> 24) & 0xFF, (server_ip >> 16) & 0xFF, 
           (server_ip >> 8) & 0xFF, server_ip & 0xFF);

    static char download_buf[65536]; 
    int downloaded_len = 0;
    
    commands_puts("Istek gonderiliyor: "); commands_puts(full_path); commands_puts("\n");
    
    // 2. HTTP İndirme
    int ok = net_http_get_to_buf(server_ip, 80, full_path, download_buf, sizeof(download_buf), &downloaded_len);

    if (ok && downloaded_len > 0) {
        // C string işlemleri yapabilmek için sonuna null koyalım (tamponda yerimiz var)
        if (downloaded_len < sizeof(download_buf)) download_buf[downloaded_len] = '\0';

        // 3. HTTP Header Ayıklama
        // "\r\n\r\n" dizisinden sonrasını buluyoruz
        char* body = strstr(download_buf, "\r\n\r\n");
        uint8_t* save_ptr = (uint8_t*)download_buf;
        uint32_t save_len = (uint32_t)downloaded_len;

        if (body) {
            body += 4; // Header ayıracını geç
            save_ptr = (uint8_t*)body;
            save_len = (uint32_t)(downloaded_len - (body - download_buf));
            printk("[BİLGİ] HTTP Header ayiklandi, sadece icerik kaydediliyor.\n");
        }
        
        printk("[TAMAM] Toplam indirme: %d byte, Kaydedilecek: %u byte\n", downloaded_len, save_len);
        
        // 4. KVXFS'e Kaydetme
        char persist_path[128] = "/persist/";
        strcat(persist_path, filename);

        if (kvxfs_write_all(persist_path, save_ptr, save_len)) {
            commands_puts("KVXFS'e kaydedildi: ");
            commands_puts(persist_path);
            commands_puts("\n");
        } else {
            commands_puts("Hata: Diske yazilamadi!\n");
        }
    } else {
        commands_puts("\nHata: HTTP istegi basarisiz.\n");
    }
}

REGISTER_COMMAND(wget, cmd_wget, "Sunucudan dosya indirir ve kaydeder.");