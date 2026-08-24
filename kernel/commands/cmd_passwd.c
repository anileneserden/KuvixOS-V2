#include <lib/commands.h>
#include <lib/string.h>
#include <init/session.h>
#include <kernel/fs/vfs.h>
#include <kernel/printk.h>
#include <kernel/memory/kmalloc.h>
#include <kernel/drivers/video/fb_console.h>
#include <kernel/drivers/input/keyboard.h>

// Shell prompt fonksiyonunu dışarıdan çağırabilmek için:
extern void shell_print_prompt(void);

// Interaktif şifre değiştirme adımları ve tamponları
static int passwd_active = 0;
static int passwd_step = 0; // 0: Eski şifre, 1: Yeni şifre
static char old_pass_buf[32];
static char new_pass_buf[32];
static int old_pos = 0;
static int new_pos = 0;

int passwd_is_active(void) {
    return passwd_active;
}

void cmd_passwd_start(int argc, char** argv) {
    (void)argc;
    (void)argv;

    user_session_t* current = session_get_current();
    if (!current || current->username[0] == '\0') {
        commands_puts("[PASSWD] Hata: Oturum acmis aktif bir kullanici bulunamadi!\n");
        return;
    }

    passwd_active = 1;
    passwd_step = 0;
    old_pos = 0;
    new_pos = 0;
    old_pass_buf[0] = '\0';
    new_pass_buf[0] = '\0';

    printk("\nCurrent password: ");
    fb_console_flush();
}

// Şifre değiştirme aktifken klavye tuşlarını yakalayan fonksiyon
int passwd_handle_scancode(uint16_t scancode) {
    if (!passwd_active) return 0;

    char c = kbd_scancode_to_ascii(scancode);
    if (c == 0) return 1;

    if (c == '\n' || c == '\r') {
        printk("\n");
        if (passwd_step == 0) {
            old_pass_buf[old_pos] = '\0';

            uint32_t max_size = 4096;
            char* buf = (char*)kmalloc(max_size);
            if (!buf) {
                printk("[PASSWD] Bellek hatasi!\n");
                passwd_active = 0;
                shell_print_prompt();
                return 1;
            }

            uint32_t nread = 0;
            if (!vfs_read_all("/etc/passwd", (uint8_t*)buf, max_size, &nread) || nread == 0) {
                printk("[PASSWD] /etc/passwd okunamadi!\n");
                kfree(buf);
                passwd_active = 0;
                shell_print_prompt();
                return 1;
            }
            buf[nread] = '\0';

            user_session_t* current = session_get_current();
            char* line = buf;
            int password_matched = 0;

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

                if (strcmp(fields[0], current->username) == 0) {
                    if (strcmp(fields[4], old_pass_buf) == 0) {
                        password_matched = 1;
                    }
                    break;
                }

                if (!newline) break;
                line = newline + 1;
            }
            kfree(buf);

            if (!password_matched) {
                printk("[PASSWD] Hatali sifre!\n");
                passwd_active = 0;
                shell_print_prompt();
                return 1;
            }

            passwd_step = 1;
            printk("New password: ");
        } 
        else if (passwd_step == 1) {
            new_pass_buf[new_pos] = '\0';

            uint32_t max_size = 4096;
            char* buf = (char*)kmalloc(max_size);
            if (!buf) {
                printk("[PASSWD] Bellek hatasi!\n");
                passwd_active = 0;
                shell_print_prompt();
                return 1;
            }

            uint32_t nread = 0;
            if (!vfs_read_all("/etc/passwd", (uint8_t*)buf, max_size, &nread)) {
                printk("[PASSWD] Dosya okunamadi!\n");
                kfree(buf);
                passwd_active = 0;
                shell_print_prompt();
                return 1;
            }
            buf[nread] = '\0';

            user_session_t* current = session_get_current();
            char* new_buf = (char*)kmalloc(max_size);
            if (!new_buf) {
                kfree(buf);
                passwd_active = 0;
                shell_print_prompt();
                return 1;
            }
            new_buf[0] = '\0';

            char* line = buf;
            int updated = 0;

            // Satır satır güvenli güncelleme döngüsü
            while (*line) {
                char line_buf[256];
                int i = 0;
                while (*line && *line != '\n' && *line != '\r' && i < (int)sizeof(line_buf) - 1) {
                    line_buf[i++] = *line++;
                }
                line_buf[i] = '\0';

                while (*line == '\n' || *line == '\r') {
                    line++;
                }

                if (line_buf[0] == '#' || line_buf[0] == '\0') {
                    strcat(new_buf, line_buf);
                    strcat(new_buf, "\n");
                    continue;
                }

                char username_field[64];
                int u_idx = 0;
                char* p = line_buf;
                while (*p && *p != ':' && u_idx < 63) {
                    username_field[u_idx++] = *p++;
                }
                username_field[u_idx] = '\0';

                if (strcmp(username_field, current->username) == 0) {
                    char fields[4][64];
                    int f = 0, c = 0;
                    p = line_buf;
                    
                    while (*p && f < 4) {
                        if (*p == ':') {
                            fields[f][c] = '\0';
                            f++;
                            c = 0;
                        } else {
                            if (c < 63) fields[f][c++] = *p;
                        }
                        p++;
                    }
                    fields[f][c] = '\0';

                    char modified_line[256];
                    ksprintf(modified_line, "%s:%s:%s:%s:%s\n", 
                             fields[0], fields[1], fields[2], fields[3], new_pass_buf);
                    strcat(new_buf, modified_line);
                    updated = 1;
                } else {
                    strcat(new_buf, line_buf);
                    strcat(new_buf, "\n");
                }
            }

            if (updated) {
                uint32_t len = strlen(new_buf);
                
                // 🔍 DISK'E YAZILMADAN ÖNCE BUFFER'I GÖRELİM:
                printk("\n[DEBUG] Yazilacak yeni icerik:\n%s\n", new_buf);

                int write_res = vfs_write_all("/etc/passwd", (uint8_t*)new_buf, len);
                printk("[DEBUG] vfs_write_all sonucu: %d\n", write_res);
                
                if (write_res > 0) {
                    printk("Sifre basariyla degistirildi.\n");
                } else {
                    printk("[PASSWD] Hata: Dosyaya yazilamadi!\n");
                }
            } else {
                printk("[PASSWD] Hata: Kullanici dosyasinda bulunamadi!\n");
            }

            kfree(buf);
            kfree(new_buf);
            passwd_active = 0;
            
            shell_print_prompt();
        }
    }
    else if (c == '\b' || c == 127) {
        if (passwd_step == 0 && old_pos > 0) {
            old_pos--;
            old_pass_buf[old_pos] = '\0';
            printk("\b \b");
        } else if (passwd_step == 1 && new_pos > 0) {
            new_pos--;
            new_pass_buf[new_pos] = '\0';
            printk("\b \b");
        }
    } 
    else {
        if (passwd_step == 0) {
            if (old_pos < (int)(sizeof(old_pass_buf) - 1)) {
                old_pass_buf[old_pos++] = c;
                printk("*");
            }
        } else if (passwd_step == 1) {
            if (new_pos < (int)(sizeof(new_pass_buf) - 1)) {
                new_pass_buf[new_pos++] = c;
                printk("*");
            }
        }
    }

    fb_console_flush();
    return 1;
}

REGISTER_COMMAND(passwd, cmd_passwd_start, "Aktif kullanicinin sifresini degistirir");