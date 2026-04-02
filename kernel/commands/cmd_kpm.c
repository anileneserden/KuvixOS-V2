#include <kernel/drivers/net/net.h>
#include <kernel/fs/kvxfs.h>
#include <lib/commands.h>
#include <lib/string.h>

#include <stdint.h>

#define KPM_SERVER_IP   ((10u << 24) | (0u << 16) | (2u << 8) | 2u)
#define KPM_SERVER_PORT 8080u
#define KPM_INDEX_PATH  "/packages_list.txt"
#define KPM_INDEX_DST   "/persist/kpm/packages_list.txt"

static int kpm_http_extract_body(char* response, int response_len, char** out_body, int* out_body_len) {
    char* body;

    if (!response || response_len <= 0 || !out_body || !out_body_len) {
        return 0;
    }

    body = strstr(response, "\r\n\r\n");
    if (body) {
        body += 4;
    } else {
        body = strstr(response, "\n\n");
        if (body) {
            body += 2;
        }
    }

    if (!body || body < response || body > (response + response_len)) {
        return 0;
    }

    *out_body = body;
    *out_body_len = response_len - (int)(body - response);
    return *out_body_len >= 0;
}

static void kpm_print_usage(void) {
    commands_puts("Kullanim:\n");
    commands_puts("  kpm update [-y]              - Paket listesini sunucudan guncelle\n");
    commands_puts("  kpm install <paket> [-y]     - Paketi kur\n");
}

static void kpm_update(void) {
    static char response_buf[32768];
    char* body;
    int response_len = 0;
    int body_len = 0;

    memset(response_buf, 0, sizeof(response_buf));

    commands_printf("[kpm] Paket listesi indiriliyor: http://10.0.2.2:%u%s\n",
                    (uint32_t)KPM_SERVER_PORT, KPM_INDEX_PATH);

    if (!net_http_get_to_buf(KPM_SERVER_IP, (uint16_t)KPM_SERVER_PORT,
                             KPM_INDEX_PATH, response_buf,
                             (int)sizeof(response_buf), &response_len)) {
        commands_puts("[kpm] Hata: Sunucudan paket listesi alinamadi.\n");
        return;
    }

    if (!kpm_http_extract_body(response_buf, response_len, &body, &body_len) || body_len <= 0) {
        commands_puts("[kpm] Hata: HTTP yaniti gecerli bir govde icermiyor.\n");
        return;
    }

    kvxfs_mkdir("/persist/kpm");

    if (!kvxfs_write_all(KPM_INDEX_DST, (const uint8_t*)body, (uint32_t)body_len)) {
        commands_puts("[kpm] Hata: Paket listesi /persist/kpm altina yazilamadi.\n");
        return;
    }

    commands_printf("[kpm] Guncellendi: %s (%d byte)\n", KPM_INDEX_DST, body_len);
}

/* packages_list.txt satirlarini "ad=yol" formatinda parse eder.
   Bulunan yolu out_path'e kopyalar; basariliysa 1, aksi halde 0 doner. */
static int kpm_find_package(const char* list, const char* pkg_name,
                             char* out_path, int out_cap) {
    const char* p = list;
    int name_len = (int)strlen(pkg_name);

    while (p && *p) {
        if (strncmp(p, pkg_name, (size_t)name_len) == 0 && p[name_len] == '=') {
            const char* val = p + name_len + 1;
            const char* eol = strchr(val, '\n');
            int val_len = eol ? (int)(eol - val) : (int)strlen(val);

            if (val_len > 0 && val[val_len - 1] == '\r')
                val_len--;

            if (val_len <= 0 || val_len >= out_cap)
                return 0;

            memcpy(out_path, val, (size_t)val_len);
            out_path[val_len] = '\0';
            return 1;
        }

        p = strchr(p, '\n');
        if (p) p++;
    }

    return 0;
}

static void kpm_install(const char* pkg_name) {
    static char list_buf[8192];
    static char pkg_path[512];
    static char response_buf[65536];
    static char save_path[256];
    uint32_t list_len = 0;
    char* body;
    int body_len = 0;
    int response_len = 0;

    if (!kvxfs_read_all(KPM_INDEX_DST, (uint8_t*)list_buf,
                        (uint32_t)(sizeof(list_buf) - 1), &list_len) || list_len == 0) {
        commands_puts("[kpm] Hata: Paket listesi bulunamadi. Once 'kpm update' calistirin.\n");
        return;
    }
    list_buf[list_len] = '\0';

    if (!kpm_find_package(list_buf, pkg_name, pkg_path, (int)sizeof(pkg_path))) {
        commands_printf("[kpm] Hata: '%s' paketi listede bulunamadi.\n", pkg_name);
        return;
    }

    commands_printf("[kpm] Kuruluyor: %s\n", pkg_name);
    commands_printf("[kpm] Sunucu yolu: '%s'\n", pkg_path);

    memset(response_buf, 0, sizeof(response_buf));
    if (!net_http_get_to_buf(KPM_SERVER_IP, (uint16_t)KPM_SERVER_PORT,
                             pkg_path, response_buf,
                             (int)sizeof(response_buf), &response_len)) {
        commands_puts("[kpm] Hata: Dosya sunucudan indirilemedi.\n");
        commands_puts("[kpm] Ipucu: 'kpm update' tekrar calistirin veya sunucuyu kontrol edin.\n");
        return;
    }

    if (!kpm_http_extract_body(response_buf, response_len, &body, &body_len) || body_len <= 0) {
        commands_puts("[kpm] Hata: HTTP yaniti gecerli degil.\n");
        return;
    }

    kvxfs_mkdir("/persist/kpm/packages");

    strcpy(save_path, "/persist/kpm/packages/");
    strcat(save_path, pkg_name);

    if (!kvxfs_write_all(save_path, (const uint8_t*)body, (uint32_t)body_len)) {
        commands_printf("[kpm] Hata: Dosya kaydedilemedi: %s\n", save_path);
        return;
    }

    commands_printf("[kpm] Kuruldu: %s (%d byte)\n", save_path, body_len);
}

static void cmd_kpm(int argc, char** argv) {
    if (argc < 2) {
        kpm_print_usage();
        return;
    }

    if (strcmp(argv[1], "update") == 0) {
        kpm_update();
        return;
    }

    if (strcmp(argv[1], "install") == 0) {
        if (argc < 3) {
            commands_puts("[kpm] Kullanim: kpm install <paket> [-y]\n");
            return;
        }
        kpm_install(argv[2]);
        return;
    }

    commands_printf("[kpm] Bilinmeyen alt komut: %s\n", argv[1]);
    kpm_print_usage();
}

REGISTER_COMMAND(kpm, cmd_kpm, "Paket yoneticisi komutlari (ilk: kpm update)");