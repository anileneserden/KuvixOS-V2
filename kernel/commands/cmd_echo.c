#include <kernel/fs/fat.h>
#include <kernel/fs/vfs.h>
#include <lib/commands.h>
#include <lib/string.h>

static void strip_surrounding_quotes(char* s) {
    if (!s) return;

    int len = (int)strlen(s);
    if (len < 2) return;

    if ((s[0] == '"' && s[len - 1] == '"') ||
        (s[0] == '\'' && s[len - 1] == '\'')) {
        memmove(s, s + 1, (size_t)(len - 2));
        s[len - 2] = 0;
    }
}

static void unescape_newlines(char* s) {
    if (!s) return;

    char* r = s;
    char* w = s;

    while (*r) {
        if (r[0] == '\\' && r[1] == 'n') {
            *w++ = '\n';
            r += 2;
            continue;
        }
        *w++ = *r++;
    }

    *w = 0;
}

void cmd_echo(int argc, char** argv) {
    int redir_index = -1;
    int append_mode = 0;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], ">>") == 0) {
            redir_index = i;
            append_mode = 1;
            break;
        }
        if (strcmp(argv[i], ">") == 0) {
            redir_index = i;
            append_mode = 0;
            break;
        }
    }

    if (redir_index > 0 && redir_index + 1 < argc) {
        const char* path = argv[redir_index + 1];
        char text[512];
        text[0] = 0;

        for (int i = 1; i < redir_index; i++) {
            if ((int)strlen(text) + 1 >= (int)sizeof(text)) break;
            if (i > 1) strncat(text, " ", sizeof(text) - 1 - strlen(text));
            strncat(text, argv[i], sizeof(text) - 1 - strlen(text));
        }

        strip_surrounding_quotes(text);
        unescape_newlines(text);

        if (strncmp(path, "/fat/", 5) == 0) {
            const char* fat_sub = path + 5;
            int ok = 0;

            if (!fat_path_exists(fat_sub)) {
                if (!fat_create_file_path(fat_sub)) {
                    commands_puts("Hata: FAT dosyasi olusturulamadi: ");
                    commands_puts(path);
                    commands_puts("\n");
                    return;
                }
            }

            if (append_mode) {
                uint8_t scratch[2];
                uint32_t old_size = 0;
                int need_newline = 0;

                if (fat_read_file_path(fat_sub, scratch, sizeof(scratch), &old_size) && old_size > 0) {
                    need_newline = 1;
                }

                if (need_newline) {
                    char append_buf[544];
                    append_buf[0] = '\n';
                    append_buf[1] = 0;
                    strncat(append_buf, text, sizeof(append_buf) - 1 - strlen(append_buf));
                    ok = fat_append_file_path(fat_sub, (const uint8_t*)append_buf, (uint32_t)strlen(append_buf));
                } else {
                    ok = fat_append_file_path(fat_sub, (const uint8_t*)text, (uint32_t)strlen(text));
                }
            } else {
                ok = fat_write_file_path(fat_sub, (const uint8_t*)text, (uint32_t)strlen(text));
            }

            if (ok) {
                commands_puts("Yazildi: ");
                commands_puts(path);
                commands_puts("\n");
            } else {
                commands_puts("Hata: FAT dosyasina yazilamadi: ");
                commands_puts(path);
                commands_puts("\n");
            }
            return;
        }

        if (!append_mode) {
            if (vfs_write_all(path, (const uint8_t*)text, (uint32_t)strlen(text))) {
                commands_puts("Yazildi: ");
                commands_puts(path);
                commands_puts("\n");
            } else {
                commands_puts("Hata: Dosyaya yazilamadi: ");
                commands_puts(path);
                commands_puts("\n");
            }
        } else {
            commands_puts("Hata: >> su anda sadece /fat altinda destekleniyor.\n");
        }
        return;
    }

    for (int i = 1; i < argc; i++) {
        commands_puts(argv[i]);
        if (i < argc - 1) commands_puts(" ");
    }
    commands_puts("\n");
}

REGISTER_COMMAND(echo, cmd_echo, "Metni ekrana yazdırır");