#include <lib/shell.h>
#include <kernel/printk.h>
#include <kernel/kbd.h>
#include <lib/commands.h>
#include <kernel/drivers/video/fb_console.h>
#include <kernel/drivers/input/keyboard.h>
#include <lib/string.h>
#include <stdint.h>
#include <stdbool.h>
#include <kernel/user.h>
#include <kernel/drivers/rtc/rtc.h>
#include <kernel/power.h>

/* Shell Durum Yönetimi */
typedef enum {
    STATE_LOGIN,
    STATE_PASSWORD,
    STATE_NORMAL
} shell_state_t;

static shell_state_t g_shell_state = STATE_NORMAL;
static bool g_auth_required = true; // ✅ Burayı false yaparsan şifre sormaz

static char g_username[32] = "";
static char g_hostname[32] = "kuvix";
static char g_cwd[128]     = "/home";

// Shutdown takibi için hedef zaman değişkenleri
int g_shutdown_target_hour = -1;
int g_shutdown_target_min  = -1;

/* Geçici Giriş Tamponları */
static char g_input_user[32];
static char g_input_pass[32];

static char g_line[128];
static int  g_len = 0;

extern char kbd_scancode_to_ascii(uint8_t sc);

// ---------------------------------------------------------
// Komut Çıktı Yönlendirme Fonksiyonları (Eksik olanlar)
// ---------------------------------------------------------

static void shell_cmd_out(void* u, const char* s) {
    (void)u;
    if (!s) return;
    printk("%s", s);
    fb_console_flush();
}

static void shell_cmd_clear(void* u) {
    (void)u;
    fb_console_clear();
    fb_console_flush();
}

// ---------------------------------------------------------

static void shell_print_prompt(void) {
    // ✅ Eğer login ekranındaysak prompt basılmasın
    if (g_shell_state == STATE_LOGIN || g_shell_state == STATE_PASSWORD) return;

    fb_console_set_color(0x0000FF00, 0x00000000); // Yeşil
    
    // user.c'deki yeni formatı kullanmak daha iyi olur ama senin mevcut yapın:
    printk("%s", g_username);
    printk("@%s", g_hostname);
    printk(":%s", g_cwd);
    
    fb_console_set_color(0x00FFFFFF, 0x00000000); // Beyaz
    printk("$ ");
    fb_console_flush();
}

void shell_init(void) {
    g_len = 0;
    memset(g_line, 0, sizeof(g_line));

    // Varsayılan kullanıcıyı al
    const char* disk_user = user_get_current_name();
    strncpy(g_username, (disk_user ? disk_user : "root"), 31);

    if (g_auth_required) {
        g_shell_state = STATE_LOGIN;
        fb_console_clear();
        fb_console_set_color(0x0000FF00, 0x00000000);
        printk("KuvixOS Login Manager\n");
        printk("---------------------\n");
        printk("Username: ");
    } else {
        g_shell_state = STATE_NORMAL;
        shell_print_prompt();
    }
    fb_console_flush();
}

void shell_handle_scancode(uint16_t ev) {
    uint8_t sc = (uint8_t)(ev & 0xFF);
    if (sc & 0x80) return;

    char c = kbd_scancode_to_ascii(sc);
    
    /* Fallback (Eğer tabloda yoksa manuel eşleştirme) */
    if (!c) {
        if (sc == 0x1E) c = 'a';
        else if (sc == 0x30) c = 'b';
        else if (sc == 0x1C) c = '\n';
        else if (sc == 0x0E) c = '\b';
        else if (sc == 0x39) c = ' ';
    }

    if (!c) return;

    /* ENTER TUŞU */
    if (c == '\n' || c == '\r') {
        g_line[g_len] = '\0';
        printk("\n");

        if (g_shell_state == STATE_LOGIN) {
            strncpy(g_input_user, g_line, 31);
            g_len = 0;
            g_shell_state = STATE_PASSWORD;
            printk("Password: ");
        } 
        else if (g_shell_state == STATE_PASSWORD) {
            strncpy(g_input_pass, g_line, 31);
            g_len = 0;

            if (user_authenticate(g_input_user, g_input_pass)) {
                strncpy(g_username, g_input_user, 31);
                g_shell_state = STATE_NORMAL;
                printk("Welcome to KuvixOS, %s!\n\n", g_username);
                shell_print_prompt();
            } else {
                printk("Login incorrect!\n\nUsername: ");
                g_shell_state = STATE_LOGIN;
            }
        } 
        else {
            if (g_len > 0) {
                commands_set_output(shell_cmd_out, NULL);
                commands_set_clear(shell_cmd_clear, NULL);
                
                commands_execute(g_line);
            }
            
            g_len = 0;

            // ✅ KRİTİK DEĞİŞİKLİK BURADA:
            // Eğer komut (logout) shell durumunu STATE_LOGIN yaptıysa, 
            // prompt basma ve direkt çık!
            if (g_shell_state == STATE_LOGIN) {
                fb_console_flush();
                return; 
            }

            shell_print_prompt();
        }
        fb_console_flush();
        return;
    }

    /* BACKSPACE */
    if (c == '\b' || (uint8_t)c == 8 || (uint8_t)c == 127) {
        if (g_len > 0) {
            g_len--;
            printk("\b \b");
            fb_console_flush();
        }
        return;
    }

    /* KARAKTER YAZMA */
    if ((uint8_t)c >= 32 && g_len < 127) {
        g_line[g_len++] = c;
        
        if (g_shell_state == STATE_PASSWORD) {
            printk("*"); // Şifreyi gizle
        } else {
            printk("%c", c);
        }
        fb_console_flush();
    }
}

void shell_tick(void) {
    // Eğer bir kapatma planı yapılmışsa (yani -1 değilse)
    if (g_shutdown_target_min != -1) {
        rtc_datetime_t now;
        
        // RTC'den mevcut saati oku
        if (rtc_read_datetime(&now)) {
            // Hedef saat ve dakikaya ulaştık mı?
            if (now.hour == g_shutdown_target_hour && now.min == g_shutdown_target_min) {
                
                // Kullanıcıya son bir mesaj bas
                fb_console_set_color(0x00FF0000, 0x00000000); // Kırmızı
                printk("\n\n[!] ZAMAN DOLDU. SISTEM KAPATILIYOR...\n");
                fb_console_flush();
                
                // Sonsuz döngüye girmemesi için hedefi temizle
                g_shutdown_target_min = -1;
                g_shutdown_target_hour = -1;
                
                // Gerçek kapatma komutu
                power_shutdown();
            }
        }
    }
}

void shell_logout(void) {
    g_shell_state = STATE_LOGIN;
    fb_console_clear();
    fb_console_set_color(0x0000FF00, 0x00000000); // Yeşil
    printk("KuvixOS Login Manager (Oturum Kapatildi)\n");
    printk("---------------------\n");
    printk("Username: ");
    fb_console_flush();
}