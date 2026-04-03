#include <kernel/kef.h>

#include <kernel/fs/vfs.h>
#include <lib/commands.h>
#include <lib/string.h>
#include <stdint.h>

static void kef_terminal_write(void* user, const char* s) {
    (void)user;
    commands_puts(s);
}

static int kef_has_suffix(const char* s, const char* suffix) {
    if (!s || !suffix) return 0;

    int sl = (int)strlen(s);
    int pl = (int)strlen(suffix);
    if (sl < pl) return 0;

    return strcmp(s + (sl - pl), suffix) == 0;
}

static void kef_resolve_path(char* out, int out_sz, const char* in) {
    out[0] = 0;
    if (!out || out_sz <= 0 || !in || !in[0]) return;

    if (in[0] == '/') {
        strncpy(out, in, (size_t)out_sz - 1);
        out[out_sz - 1] = 0;
        return;
    }

    const char* cwd = commands_get_cwd();
    strncpy(out, cwd ? cwd : "/", (size_t)out_sz - 1);
    out[out_sz - 1] = 0;

    if (out[0]) {
        int len = (int)strlen(out);
        if (len > 0 && out[len - 1] != '/') {
            strncat(out, "/", (size_t)(out_sz - 1 - (int)strlen(out)));
        }
    }

    if (in[0] == '.' && in[1] == '/') in += 2;
    strncat(out, in, (size_t)(out_sz - 1 - (int)strlen(out)));
}

static int kef_validate_header(const kef_header_t* hdr, uint32_t size) {
    if (!hdr) return 0;
    if (hdr->magic[0] != KEF_MAGIC_0 ||
        hdr->magic[1] != KEF_MAGIC_1 ||
        hdr->magic[2] != KEF_MAGIC_2 ||
        hdr->magic[3] != KEF_MAGIC_3) return 0;
    if (hdr->version != KEF_VERSION_1) return 0;
    if (hdr->code_offset > size || hdr->str_offset > size) return 0;
    if (hdr->code_size > size || hdr->str_size > size) return 0;
    if (hdr->code_offset + hdr->code_size > size) return 0;
    if (hdr->str_offset + hdr->str_size > size) return 0;
    return 1;
}

static const char* kef_string_at(const uint8_t* str_base, uint32_t str_size, uint32_t off) {
    if (!str_base || off >= str_size) return NULL;

    const char* s = (const char*)(str_base + off);
    for (uint32_t i = off; i < str_size; i++) {
        if (str_base[i] == 0) return s;
    }
    return NULL;
}

static uint32_t kef_read_u32(const uint8_t* p) {
    return ((uint32_t)p[0]) |
           ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) |
           ((uint32_t)p[3] << 24);
}

int kef_run_buffer(const uint8_t* data, uint32_t size, const kef_host_t* host) {
    kef_host_t default_host;
    kef_header_t hdr;

    if (!data || size < sizeof(kef_header_t)) {
        commands_puts("KEF hata: Dosya cok kucuk.\n");
        return 0;
    }

    memcpy(&hdr, data, sizeof(hdr));
    if (!kef_validate_header(&hdr, size)) {
        commands_puts("KEF hata: Gecersiz dosya formati.\n");
        return 0;
    }

    if (hdr.app_kind != KEF_APP_TERMINAL) {
        commands_puts("KEF hata: Bu surum sadece terminal app calistirir.\n");
        return 0;
    }

    default_host.write = kef_terminal_write;
    default_host.user = NULL;
    if (!host) host = &default_host;
    if (!host->write) {
        commands_puts("KEF hata: Host write callback yok.\n");
        return 0;
    }

    const uint8_t* ip = data + hdr.code_offset;
    const uint8_t* code_end = ip + hdr.code_size;
    const uint8_t* str_base = data + hdr.str_offset;

    while (ip < code_end) {
        uint8_t op = *ip++;

        if (op == KEF_OP_NOP) continue;

        if (op == KEF_OP_PRINT) {
            if ((uint32_t)(code_end - ip) < 4) {
                commands_puts("KEF hata: PRINT operand eksik.\n");
                return 0;
            }

            uint32_t off = kef_read_u32(ip);
            ip += 4;

            const char* s = kef_string_at(str_base, hdr.str_size, off);
            if (!s) {
                commands_puts("KEF hata: String tablosu bozuk.\n");
                return 0;
            }

            host->write(host->user, s);
            continue;
        }

        if (op == KEF_OP_EXIT) {
            return 1;
        }

        commands_puts("KEF hata: Bilinmeyen opcode.\n");
        return 0;
    }

    return 1;
}

int kef_run_path_terminal(const char* path) {
    uint8_t* data = NULL;
    uint32_t size = 0;

    if (!path || !path[0]) {
        commands_puts("KEF hata: path bos.\n");
        return 0;
    }

    if (!vfs_read_all_alloc(path, &data, &size)) {
        commands_puts("KEF hata: Dosya okunamadi: ");
        commands_puts(path);
        commands_puts("\n");
        return 0;
    }

    int ok = kef_run_buffer(data, size, NULL);
    vfs_free_alloc(data);
    return ok;
}

int kef_try_run_command(int argc, char** argv) {
    char path[VFS_PATH_MAX];

    if (argc <= 0 || !argv || !argv[0] || !argv[0][0]) return 0;
    if (!kef_has_suffix(argv[0], ".kef")) return 0;

    kef_resolve_path(path, (int)sizeof(path), argv[0]);
    if (!kef_run_path_terminal(path)) {
        commands_puts("KEF calistirma basarisiz.\n");
    }
    return 1;
}