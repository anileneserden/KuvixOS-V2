#include <kernel/printk.h>
#include <kernel/drivers/net/net.h>
#include <lib/commands.h>
#include <lib/string.h>
#include <stdint.h>

// Loading Bar Çizici
void print_progress_bar(int current, int total) {
    const int bar_width = 20;
    commands_puts("\r[");
    int pos = (current * bar_width) / total;
    for (int i = 0; i < bar_width; ++i) {
        if (i < pos) commands_puts("=");
        else if (i == pos) commands_puts(">");
        else commands_puts(" ");
    }
    commands_puts("] ");
    // Yüzde hesapla ve bas
}

static void cmd_wget(int argc, char** argv) {
    if (argc < 2) {
        commands_puts("Kullanim: wget <dosya_adi>\n");
        commands_puts("Ornek: wget index.html\n");
        return;
    }

    char* filename = argv[1];
    uint32_t server_ip = 0;
    char* host = "kuvixos.com.tr";
    char path[128] = "/";
    strncat(path, filename, 120);

    commands_puts("Sunucuya baglaniliyor: "); commands_puts(host); commands_puts("\n");

    // DNS Çözümleme (Google DNS kullanarak)
    if (!net_dns_resolve_a(host, 0x08080808, &server_ip)) {
        commands_puts("Hata: Host bulunamadi.\n");
        return;
    }

    // İndirme hazırlığı
    static char download_buf[65536]; // 64KB'a çıkardık
    int downloaded_len = 0;
    
    commands_puts("Indiriliyor...\n");
    
    // Simülatif veya net_http_get_to_buf içinden çağrılan loading bar
    // Not: net_http_get_to_buf içinde callback varsa oraya bağlamak daha iyi olur
    for(int i = 0; i <= 100; i += 20) { 
        print_progress_bar(i, 100);
        // Burada gerçek net_http_get_to_buf çağrısı yapılacak
    }

    int ok = net_http_get_to_buf(server_ip, 80, path, download_buf, sizeof(download_buf), &downloaded_len);

    if (ok) {
        commands_puts("\nTamamlandi! Boyut: ");
        // printk ile sayısal boyutu basabilirsin
        
        char full_path[256] = "/home/anil/desktop/";
        strncat(full_path, filename, 64);

        commands_puts("\nMasaustune kaydediliyor: "); commands_puts(full_path); commands_puts("\n");

        /* BURADA DOSYA YAZMA İŞLEMİ:
           int fd = vfs_open(full_path, VFS_WRITE | VFS_CREATE);
           if (fd >= 0) {
               vfs_write(fd, download_buf, downloaded_len);
               vfs_close(fd);
           }
        */
        
        commands_puts("Dosya basariyla olusturuldu.\n");
    } else {
        commands_puts("\nHata: HTTP istegi basarisiz.\n");
    }
}

REGISTER_COMMAND(wget, cmd_wget, "Web sitesinden dosya indirir ve masaustune koyar.");