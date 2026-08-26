#include <init/session.h>
#include <lib/shell.h>
#include <kernel/fs/vfs.h>
#include <lib/string.h>
#include <kernel/printk.h>
#include <kernel/memory/kmalloc.h>
#include <kernel/drivers/video/fb_console.h>

// Çekirdek içerisindeki özel yükleyici imzaları (loader.c içinde tanımlı)
extern void load_desktop_module(const char* path);
extern void load_login_module(const char* path);

// Debug loglarını açmak için 1 yap, kapatmak için 0 yap
#define DEBUG_LOGIN_ENABLED 0

#if DEBUG_LOGIN_ENABLED
#define LOGIN_DEBUG(fmt, ...) printk(fmt, ##__VA_ARGS__)
#else
#define LOGIN_DEBUG(fmt, ...) ((void)0)
#endif

extern char kbd_scancode_to_ascii(uint16_t scancode);

static user_session_t current_session;
static int is_logged_in = 0;
static int require_password = 1; // Varsayılan olarak şifre istesin

// Önceki oturum takibi için değişkenler (exit desteği için)
static uint32_t prev_uid = 0;
static char prev_username[32] = {0};
static char prev_home[64] = {0};
static int has_previous_session = 0;

static char login_user_buf[32];
static char login_pass_buf[32];
static int login_step = 0; 
static int user_pos = 0;
static int pass_pos = 0;

static int simple_atoi(const char* s) {
    int res = 0;
    while (*s >= '0' && *s <= '9') {
        res = res * 10 + (*s - '0');
        s++;
    }
    return res;
}

static void load_session_config(void) {
    uint32_t max_size = 512;
    char* buf = (char*)kmalloc(max_size);
    if (!buf) return;

    uint32_t nread = 0;
    if (vfs_read_all("/sys/configs/session.cfg", (uint8_t*)buf, max_size, &nread) && nread > 0) {
        buf[nread] = '\0';
        if (strstr(buf, "isPassword=false") || strstr(buf, "ispassword=false")) {
            require_password = 0;
        } else {
            require_password = 1;
        }
    }
    kfree(buf);
}

user_session_t* session_get_current(void) {
    return &current_session;
}

void session_set_user(uint32_t uid, const char* username, const char* home) {
    current_session.uid = uid;
    if (username) {
        strncpy(current_session.username, username, sizeof(current_session.username) - 1);
    }
    if (home) {
        strncpy(current_session.home_dir, home, sizeof(current_session.home_dir) - 1);
    }
}

// Önceki oturum yönetim fonksiyonları
int session_has_previous(void) {
    return has_previous_session;
}

int session_is_logged_in(void) {
    return is_logged_in;
}

void session_save_previous(uint32_t uid, const char* username, const char* home) {
    prev_uid = uid;
    if (username) strncpy(prev_username, username, sizeof(prev_username) - 1);
    if (home) strncpy(prev_home, home, sizeof(prev_home) - 1);
    has_previous_session = 1;
}

void session_restore_previous(void) {
    if (!has_previous_session) return;

    // Önceki kullanıcı bilgilerini geri yükle
    session_set_user(prev_uid, prev_username, prev_home);
    vfs_set_cwd(prev_home);
    shell_set_username(prev_username);
    shell_set_hostname("kuvix");

    printk("[SESSION] %s kullanicisina geri donuldu.\n", prev_username);
    has_previous_session = 0;
}

void session_logout(void) {
    has_previous_session = 0;
    is_logged_in = 0;
    
    // Oturumdaki kullanıcı bilgilerini temizle
    session_set_user(0, "", "");
    
    // Giriş değişkenlerini sıfırla
    login_step = 0;
    user_pos = 0;
    pass_pos = 0;
    
    fb_console_clear();
    printk("[SESSION] Oturum kapatildi.\n\n");
    
    // Diskleri ve dosya sistemini sıfırlayan session_init() yerine 
    // doğrudan temiz bir şekilde giriş ekranını ekrana basalım:
    fb_console_set_color(0x00FFFFFF, 0x00000000);
    printk("--- KUVIX OS GIRIS SISTEMI ---\n");
    printk("Kullanici adi (login): ");
    fb_console_flush();
}

static int check_user_exists(const char* username) {
    uint32_t max_size = 2048;
    char* buf = (char*)kmalloc(max_size);
    if (!buf) return 0;

    uint32_t nread = 0;
    if (!vfs_read_all("/etc/passwd", (uint8_t*)buf, max_size, &nread) || nread == 0) {
        kfree(buf);
        return 0;
    }

    buf[nread] = '\0';
    char* line = buf;
    int exists = 0;

    while (*line) {
        char* newline = strchr(line, '\n');
        if (newline) *newline = '\0';

        int line_len = strlen(line);
        if (line_len > 0 && line[line_len - 1] == '\r') {
            line[line_len - 1] = '\0';
        }

        if (*line == '#' || *line == '\0') {
            if (!newline) break;
            line = newline + 1;
            continue;
        }

        char db_user[64];
        int c_idx = 0;
        char* p = line;
        while (*p && *p != ':' && c_idx < 63) {
            if (*p != ' ' && *p != '\t') {
                db_user[c_idx++] = *p;
            }
            p++;
        }
        db_user[c_idx] = '\0';

        if (strcmp(db_user, username) == 0) {
            exists = 1;
            break;
        }

        if (!newline) break;
        line = newline + 1;
    }

    kfree(buf);
    return exists;
}

int authenticate_user(const char* username, const char* password, int start_shell) {
    uint32_t max_size = 2048;
    char* buf = (char*)kmalloc(max_size);
    if (!buf) return 0;

    uint32_t nread = 0;
    if (!vfs_read_all("/etc/passwd", (uint8_t*)buf, max_size, &nread) || nread == 0) {
        kfree(buf);
        return 0;
    }

    buf[nread] = '\0';
    char* line = buf;
    int authenticated = 0;

    while (*line) {
        char* newline = strchr(line, '\n');
        if (newline) *newline = '\0';

        int line_len = strlen(line);
        if (line_len > 0 && line[line_len - 1] == '\r') {
            line[line_len - 1] = '\0';
        }

        if (*line == '#' || *line == '\0') {
            if (!newline) break;
            line = newline + 1;
            continue;
        }

        char fields[5][64];
        int f_idx = 0, c_idx = 0;
        char* p = line;

        while (*p && f_idx < 5) {
            if (*p == ':') {
                fields[f_idx][c_idx] = '\0';
                f_idx++;
                c_idx = 0;
            } else {
                if (c_idx < 63 && *p != ' ' && *p != '\r' && *p != '\n' && *p != '\t') {
                    fields[f_idx][c_idx++] = *p;
                }
            }
            p++;
        }
        fields[f_idx][c_idx] = '\0';

        if (f_idx >= 0) {
            int match = 0;
            if (!require_password) {
                if (strcmp(fields[0], username) == 0) {
                    match = 1;
                }
            } else {
                if (strcmp(fields[0], username) == 0) {
                    if (strcmp(fields[4], password) == 0) {
                        match = 1;
                    } else {
                        kfree(buf);
                        return 0; 
                    }
                }
            }

            if (match) {
                uint32_t uid = (f_idx >= 2) ? (uint32_t)simple_atoi(fields[2]) : 0;
                const char* home = (f_idx >= 5) ? fields[5] : "/home";
                
                session_set_user(uid, fields[0], home);
                vfs_set_cwd(home);
                shell_set_username(fields[0]);
                shell_set_hostname("kuvix");
                
                authenticated = 1;
                
                if (start_shell) {
                    printk("\n[SESSION] Giris basarili! Hosgeldiniz %s\n", fields[0]);
                    is_logged_in = 1;
                    shell_init(); 
                }
                break;
            }
        }

        if (!newline) break;
        line = newline + 1;
    }

    kfree(buf);
    return authenticated;
}

void session_init(void) {
    fb_console_clear(); 
    printk("[SESSION] Oturum yoneticisi baslatiliyor...\n");
    
    is_logged_in = 0;
    login_step = 0;
    user_pos = 0;
    pass_pos = 0;
    has_previous_session = 0;
    
    // Config dosyasından şifre gereksinimini yükle
    load_session_config();

    char is_password_str[16] = {0};
    char target_path[64] = {0};

    // session.cfg üzerinden isPassword ve modül yollarını kontrol etmeye çalışalım
    uint32_t max_size = 512;
    char* buf = (char*)kmalloc(max_size);
    if (buf) {
        uint32_t nread = 0;
        if (vfs_read_all("/sys/configs/session.cfg", (uint8_t*)buf, max_size, &nread) && nread > 0) {
            buf[nread] = '\0';
            
            // parse_session_config yerine mevcut config okuma mantığımızı veya basit arama kullanabiliriz
            if (require_password) {
                // Eğer şifreli giriş aktifse ve loginScreen tanımlıysa yükle
                if (strstr(buf, "loginScreen=")) {
                    char* p = strstr(buf, "loginScreen=");
                    if (p) {
                        p += 12;
                        int i = 0;
                        while (*p && *p != '\n' && *p != '\r' && i < 63) {
                            target_path[i++] = *p++;
                        }
                        target_path[i] = '\0';
                        
                        kfree(buf);
                        printk("[SESSION] Sifre aktif. Giris ekrani yukleniyor: %s\n", target_path);
                        load_login_module(target_path);
                        return;
                    }
                }
            } else {
                // Şifre kapalıysa doğrudan masaüstünü yükle
                if (strstr(buf, "desktopScreen=")) {
                    char* p = strstr(buf, "desktopScreen=");
                    if (p) {
                        p += 14;
                        int i = 0;
                        while (*p && *p != '\n' && *p != '\r' && i < 63) {
                            target_path[i++] = *p++;
                        }
                        target_path[i] = '\0';
                        
                        kfree(buf);
                        printk("[SESSION] Sifre kapali. Masaustu yukleniyor: %s\n", target_path);
                        load_desktop_module(target_path);
                        return;
                    }
                }
            }
        }
        kfree(buf);
    }

    // Fallback: Modül yolu bulunamazsa normal metin tabanlı konsol giriş döngüsüne devam et
    fb_console_set_color(0x00FFFFFF, 0x00000000);
    printk("\n--- KUVIX OS GIRIS SISTEMI ---\n");
    printk("Kullanici adi (login): ");
    fb_console_flush();
}

void session_handle_scancode(uint16_t scancode) {
    if (is_logged_in) {
        shell_handle_scancode(scancode);
        return;
    }

    if (scancode == 0x3B) {
        printk("\n[SESSION] F1 Kisayolu ile otomatik giris yapiliyor...\n");
        session_set_user(0, "anil", "/home/anil");
        vfs_set_cwd("/home/anil");
        shell_set_username("anil");
        shell_set_hostname("kuvix");
        is_logged_in = 1;
        shell_init();
        return;
    }

    char c = kbd_scancode_to_ascii(scancode);
    if (c == 0) return;

    if (c == '\n' || c == '\r') {
        printk("\n");
        if (login_step == 0) {
            login_user_buf[user_pos] = '\0';
            
            if (!check_user_exists(login_user_buf)) {
                printk("[SESSION] Boyle bir kullanici bulunamadi!\n\n");
                login_step = 0;
                user_pos = 0;
                printk("Kullanici adi (login): ");
                fb_console_flush();
                return;
            }

            if (!require_password) {
                if (!authenticate_user(login_user_buf, "", 1)) {
                    printk("[SESSION] Giris basarisiz!\n\n");
                    login_step = 0;
                    user_pos = 0;
                    printk("Kullanici adi (login): ");
                }
            } else {
                login_step = 1;
                printk("Password: ");
            }
        } 
        else if (login_step == 1) {
            login_pass_buf[pass_pos] = '\0';
            
            if (!authenticate_user(login_user_buf, login_pass_buf, 1)) {
                printk("[SESSION] Hatali sifre!\n\n");
                login_step = 0;
                user_pos = 0;
                pass_pos = 0;
                printk("Kullanici adi (login): ");
            }
        }
    }
    else if (c == '\b' || c == 127) {
        if (login_step == 0 && user_pos > 0) {
            user_pos--;
            login_user_buf[user_pos] = '\0';
            printk("\b \b"); 
        } else if (login_step == 1 && pass_pos > 0) {
            pass_pos--;
            login_pass_buf[pass_pos] = '\0';
        }
    } 
    else {
        if (login_step == 0) {
            if (user_pos < (int)(sizeof(login_user_buf) - 1)) {
                login_user_buf[user_pos++] = c;
                char tmp[2] = {c, '\0'};
                printk("%s", tmp); 
            }
        } else if (login_step == 1) {
            if (pass_pos < (int)(sizeof(login_pass_buf) - 1)) {
                login_pass_buf[pass_pos++] = c;
            }
        }
    }

    fb_console_flush(); 
}

void session_tick(void) {
    if (is_logged_in) {
        shell_tick();
    }
}