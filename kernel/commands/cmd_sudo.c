#include <lib/commands.h>
#include <lib/string.h>
#include <kernel/printk.h>
#include <init/session.h>
#include <lib/shell.h>
#include <kernel/fs/vfs.h>

extern command_t _cmd_start[];
extern command_t _cmd_end[];

extern int authenticate_user(const char* username, const char* password);

void cmd_sudo(int argc, char** argv) {
    if (argc < 3) {
        printk("Kullanim: sudo <root_sifresi> <komut>\n");
        printk("Ornek: sudo root_sifresi su\n");
        printk("Ornek: sudo root_sifresi cat /etc/passwd\n");
        return;
    }

    user_session_t* session = session_get_current();
    if (!session) {
        printk("Hata: Aktif bir oturum bulunamadi!\n");
        return;
    }

    const char* password_input = argv[1]; // İkinci parametre şifre

    // Sudo/Su için her zaman root şifresini kontrol et
    if (!authenticate_user("root", password_input)) {
        printk("Yanlis sifre!\n");
        return;
    }

    int sub_argc = argc - 2;
    char** sub_argv = &argv[2];

    // ÖZEL DURUM: "sudo <sifre> su" komutu
    if (sub_argc == 1 && strcmp(sub_argv[0], "su") == 0) {
        // Oturumu kalıcı olarak root yap
        session_set_user(0, "root", "/");
        vfs_set_cwd("/");
        shell_set_username("root");
        shell_set_hostname("kuvix");
        printk("Root oturumu acildi.\n");
        return;
    }

    // Normal komut çalıştırma (geçici root yetkisi)
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