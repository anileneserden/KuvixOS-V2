#include <ui/html/url_resolver.h>
#include <kernel/fs/vfs.h>
#include <lib/string.h>
#include <stdint.h>

#define HOSTS_PATH "/etc/hosts"
#define MAX_LINE   256

// Basit trim (sağ/sol boşluk temizler)
static void trim(char* s) {
    if (!s) return;

    // left trim
    while (*s == ' ' || *s == '\t') {
        memmove(s, s + 1, strlen(s));
    }

    // right trim
    int len = strlen(s);
    while (len > 0 && (s[len - 1] == ' ' || s[len - 1] == '\t' || s[len - 1] == '\r')) {
        s[len - 1] = 0;
        len--;
    }
}

// .local mı?
static bool ends_with_local(const char* s) {
    int l = strlen(s);
    if (l < 6) return false;
    return strcmp(s + l - 6, ".local") == 0;
}

// /etc/hosts dosyasından domain çöz
static bool resolve_from_hosts(const char* url, char* out, int out_cap) {
    vfs_file_t* f = NULL;
    if (vfs_open(HOSTS_PATH, VFS_O_RDONLY, &f) != 1)
        return false;

    char buf[2048];
    uint32_t read = 0;
    if (vfs_read(f, buf, sizeof(buf) - 1, &read) != 1) {
        vfs_close(f);
        return false;
    }

    buf[read] = 0;
    vfs_close(f);

    char* line = buf;
    while (*line) {

        char* next = strchr(line, '\n');
        if (next) {
            *next = 0;
        }

        trim(line);

        if (line[0] != '#' && strlen(line) > 0) {

            char* eq = strchr(line, '=');
            if (eq) {
                *eq = 0;
                char* key = line;
                char* value = eq + 1;

                trim(key);
                trim(value);

                if (strcmp(key, url) == 0) {
                    strncpy(out, value, out_cap - 1);
                    out[out_cap - 1] = 0;
                    return true;
                }
            }
        }

        if (!next) break;
        line = next + 1;
    }

    return false;
}

bool url_resolve_to_path(const char* url, char* out, int out_cap) {
    if (!url || !out) return false;

    // Eğer zaten absolute path ise
    if (url[0] == '/') {
        strncpy(out, url, out_cap - 1);
        out[out_cap - 1] = 0;
        return true;
    }

    // .local ise hosts çöz
    if (ends_with_local(url)) {

        char tmp[256];
        if (!resolve_from_hosts(url, tmp, sizeof(tmp)))
            return false;

        // klasör mü kontrol et
        vfs_file_t* f = NULL;
        if (vfs_open(tmp, VFS_O_RDONLY, &f) == 1) {
            vfs_close(f);

            // dosya ise direkt
            strncpy(out, tmp, out_cap - 1);
            out[out_cap - 1] = 0;
            return true;
        }

        // klasör varsay → index.html dene
        char with_index[256];
        strncpy(with_index, tmp, sizeof(with_index) - 1);
        with_index[sizeof(with_index) - 1] = 0;

        int len = strlen(with_index);
        if (len > 0 && with_index[len - 1] != '/')
            strcat(with_index, "/");

        strcat(with_index, "index.html");

        strncpy(out, with_index, out_cap - 1);
        out[out_cap - 1] = 0;
        return true;
    }

    return false;
}