#include <kernel/printk.h>
#include <kernel/drivers/net/net.h>
#include <lib/commands.h>
#include <lib/string.h>
#include <kernel/fs/vfs.h>
#include <kernel/fs/kvxfs.h>
#include <stdint.h>

static void print_progress_bar(int current, int total) {
    const int bar_width = 20;
    if (total <= 0) return;
    int percentage = (current * 100) / total;
    printk("\r[");
    int pos = (current * bar_width) / total;
    for (int i = 0; i < bar_width; ++i) {
        if (i < pos) printk("=");
        else if (i == pos) printk(">");
        else printk(" ");
    }
    printk("] %d%%", percentage);
}

static void cmd_wget(int argc, char** argv) {
    if (argc < 2) {
        printk("Kullanim: wget <dosya_adi>\n");
        return;
    }

    char* filename = argv[1];
    uint32_t server_ip = (10 << 24) | (0 << 16) | (2 << 8) | 2; 
    char path[128];
    ksprintf(path, "/%s", filename);

    static char download_buf[16384]; 
    int total_len = 0;
    memset(download_buf, 0, sizeof(download_buf));

    printk("Baglanti kuruluyor: 10.0.2.2:8080%s\n", path);

    // Tek bir çağrı, ama içeride tüm paketleri toplamalı
    int ok = net_http_get_to_buf(server_ip, 8080, path, download_buf, sizeof(download_buf), &total_len);

    if (ok && total_len > 0) {
        char* data_start = strstr(download_buf, "\r\n\r\n");
        if (data_start) data_start += 4;
        else {
            data_start = strstr(download_buf, "\n\n");
            if (data_start) data_start += 2;
        }

        if (!data_start || (total_len - (int)(data_start - download_buf)) <= 0) {
            printk("Hata: Paket yarim kaldi (%d byte). Header:\n", total_len);
            for(int i=0; i<total_len && i<128; i++) {
                if(download_buf[i] >= 32) printk("%c", download_buf[i]);
                else if(download_buf[i] == '\n') printk("\n");
            }
            return;
        }

        int actual_len = total_len - (int)(data_start - download_buf);
        char persist_path[128];
        ksprintf(persist_path, "/persist/%s", filename);
        
        kvxfs_init();
        if (kvxfs_write_all(persist_path, (uint8_t*)data_start, (uint32_t)actual_len)) {
            printk("Basarili! /persist/%s kaydedildi (%d byte).\n", filename, actual_len);
        }
    } else {
        printk("Hata: Sunucudan yanit yok.\n");
    }
}

REGISTER_COMMAND(wget, cmd_wget, "Agdan dosya indirir ve /persist altina kaydeder.");