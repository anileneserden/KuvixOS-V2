#include <init/session.h>
#include <lib/shell.h>
#include <kernel/fs/vfs.h>
#include <lib/string.h>
#include <kernel/printk.h>
#include <kernel/memory/kmalloc.h>

// Çekirdek içerisindeki özel yükleyici imzaları (loader.c içinde tanımlı)
extern void load_desktop_module(const char* path);
extern void load_login_module(const char* path);

// Config dosyasından anahtar-değer okuma yardımcısı
static int parse_session_config(const char* filepath, const char* key, char* dest, int max_len) {
    uint32_t max_size = 2048;
    char* buf = (char*)kmalloc(max_size);
    if (!buf) return 0;

    uint32_t nread = 0;
    if (!vfs_read_all(filepath, (uint8_t*)buf, max_size, &nread) || nread == 0) {
        kfree(buf);
        return 0;
    }

    buf[nread] = '\0';
    char* line = buf;
    int found = 0;

    while (*line) {
        char* newline = strchr(line, '\n');
        if (newline) *newline = '\0';

        if (strncmp(line, key, strlen(key)) == 0) {
            char* val = strchr(line, '=');
            if (val) {
                val++;
                while (*val == ' ' || *val == '\t') val++;
                
                int i = 0;
                while (val[i] && val[i] != '\r' && val[i] != '\n' && i < max_len - 1) {
                    dest[i] = val[i];
                    i++;
                }
                dest[i] = '\0';
                found = 1;
                break;
            }
        }

        if (!newline) break;
        line = newline + 1;
    }

    kfree(buf);
    return found;
}

void session_init(void) {
    printk("[SESSION] Oturum yoneticisi baslatiliyor...\n");

    char is_password[16] = {0};
    char target_path[64] = {0};

    // 1. Config dosyasından isPassword durumunu oku
    if (!parse_session_config("/sys/configs/session.cfg", "isPassword", is_password, sizeof(is_password))) {
        strcpy(is_password, "false"); // Varsayılan olarak kapalı kabul et
    }

    // 2. Şifre durumuna göre doğru yükleyiciyi ve modül yolunu seç
    if (strcmp(is_password, "true") == 0) {
        if (parse_session_config("/sys/configs/session.cfg", "loginScreen", target_path, sizeof(target_path))) {
            printk("[SESSION] Sifre aktif. Giris ekrani yukleniyor: %s\n", target_path);
            load_login_module(target_path); // LoginAPI kullanan kls loader
            return;
        }
    } else {
        if (parse_session_config("/sys/configs/session.cfg", "desktopScreen", target_path, sizeof(target_path))) {
            printk("[SESSION] Sifre kapali. Masaustu yukleniyor: %s\n", target_path);
            load_desktop_module(target_path); // DE_API kullanan kde loader
            return;
        }
    }

    // Fallback: Eğer config okunamazsa veya modül bulunamazsa normal kabuğa düş
    printk("[SESSION] Uyari: Modul yolu bulunamadi, kabuga (shell) yonlendiriliyor.\n");
}

void session_handle_scancode(uint16_t scancode) {
    // Donanımdan gelen tuş kodunu kabuğa aktarır
    shell_handle_scancode(scancode);
}

void session_tick(void) {
    // Ekran tazeleme döngüsü
    shell_tick();
}