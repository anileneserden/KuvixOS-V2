#include <lib/commands.h>
#include <lib/string.h>
#include <kernel/printk.h>
#include <init/session.h>
#include <lib/shell.h>
#include <kernel/fs/vfs.h>
#include <kernel/drivers/input/keyboard.h>
#include <kernel/drivers/video/fb_console.h>

extern command_t _cmd_start[];
extern command_t _cmd_end[];

extern int authenticate_user(const char* username, const char* password, int start_shell);

// Şifreyi yıldızla (*) maskeleyerek okuyan fonksiyon
static void sudo_get_password(char* buf, int max_len) {
    int len = 0;
    printk("[sudo] root password for %s: ", session_get_current()->username);
    fb_console_flush(); // Prompt yazısını ekrana bas
    
    while (1) {
        char c = kbd_get_char();
        if (c == '\n' || c == '\r') {
            printk("\n");
            fb_console_flush();
            break;
        } else if (c == '\b' || c == 127) {
            if (len > 0) {
                len--;
                printk("\b \b");
                fb_console_flush();
            }
        } else if (c >= 32 && c < 127 && len < max_len - 1) {
            buf[len++] = c;
            printk("*");
            fb_console_flush(); // Basılan yıldızı ekrana anında yansıt
        }
    }
    buf[len] = '\0';
}

void cmd_sudo(int argc, char** argv) {
    user_session_t* session = session_get_current();
    if (!session) {
        printk("Hata: Aktif bir oturum bulunamadi!\n");
        return;
    }

    char password_input[64];

    // ÖZEL DURUM: "sudo su" komutu
    if (argc == 1 || (argc == 2 && strcmp(argv[1], "su") == 0)) {
        sudo_get_password(password_input, sizeof(password_input));

        // 3. parametre 0 -> Ekranı temizlemeden ve shell'i resetlemeden doğrula
        if (!authenticate_user("root", password_input, 0)) {
            printk("Uzgunum, basarisiz sifre denemesi.\n");
            return;
        }

        // Oturumu kalıcı olarak root yap
        session_set_user(0, "root", "/root");
        vfs_set_cwd("/root");
        shell_set_username("root");
        shell_set_hostname("kuvix");
        
        return;
    }

    // Geriye dönük parametreli kullanım desteği
    if (argc < 3) {
        printk("Kullanim: sudo su\n");
        printk("Veya: sudo <sifre> <komut>\n");
        return;
    }

    const char* direct_pwd = argv[1];
    if (!authenticate_user("root", direct_pwd, 0)) {
        printk("Yanlis sifre!\n");
        return;
    }

    int sub_argc = argc - 2;
    char** sub_argv = &argv[2];

    command_t* target_cmd = 0;
    for (command_t* cmd = _cmd_start; cmd < _cmd_end; cmd++) {
        if (cmd->name && strcmp(sub_argv[0], cmd->name) == 0) {
            target_cmd = cmd;
            break;
        }
    }

    if (!target_cmd) {
        printk("sudo: Bilinmeyen komut: '%s'\n", sub_argv[0]);
        return;
    }

    uint32_t old_uid = session->uid;
    session->uid = 0;
    target_cmd->fn(sub_argc, sub_argv);
    session->uid = old_uid;
}

REGISTER_COMMAND(sudo, cmd_sudo, "Executes a command with root privileges");