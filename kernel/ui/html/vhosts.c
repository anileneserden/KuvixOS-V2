#include <ui/html/vhosts.h>
#include <kernel/fs/vfs.h>
#include <lib/string.h>
#include <kernel/printk.h>

static void trim_left(char** s) {
    while (*s && **s && (**s == ' ' || **s == '\t' || **s == '\r' || **s == '\n')) (*s)++;
}

static void trim_right(char* s) {
    if (!s) return;
    int n = (int)strlen(s);
    while (n > 0) {
        char c = s[n - 1];
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n') { s[n - 1] = 0; n--; }
        else break;
    }
}

static bool starts_with(const char* s, const char* pfx) {
    if (!s || !pfx) return false;
    size_t a = strlen(s), b = strlen(pfx);
    if (a < b) return false;
    return (strncmp(s, pfx, b) == 0);
}

static void copy_token(char* out, int cap, const char* v) {
    if (!out || cap <= 0) return;
    out[0] = 0;
    if (!v) return;

    // strip optional trailing ';'
    char tmp[256];
    strncpy(tmp, v, sizeof(tmp) - 1);
    tmp[sizeof(tmp) - 1] = 0;
    trim_right(tmp);
    int n = (int)strlen(tmp);
    if (n > 0 && tmp[n - 1] == ';') tmp[n - 1] = 0;
    trim_right(tmp);

    strncpy(out, tmp, (size_t)cap - 1);
    out[cap - 1] = 0;
}

const vhost_entry_t* vhosts_find(const vhosts_table_t* t, const char* host) {
    if (!t || !host || !host[0]) return NULL;
    for (int i = 0; i < t->count; i++) {
        if (strcmp(t->items[i].host, host) == 0) return &t->items[i];
    }
    return NULL;
}

// very small line reader
static bool read_line(const char* buf, uint32_t sz, uint32_t* io, char* out, int cap) {
    if (!buf || !io || !out || cap <= 0) return false;
    if (*io >= sz) return false;

    int p = 0;
    while (*io < sz && p < cap - 1) {
        char c = buf[*io];
        (*io)++;
        if (c == '\n') break;
        out[p++] = c;
    }
    out[p] = 0;
    return true;
}

bool vhosts_load(const char* path, vhosts_table_t* out) {
    if (!out) return false;
    memset(out, 0, sizeof(*out));
    if (!path) path = "/etc/vhosts.conf";

    uint8_t* buf = 0;
    uint32_t sz = 0;
    if (!vfs_read_all_alloc(path, &buf, &sz)) {
        printk("[vhosts] can't read %s\n", path);
        return false;
    }

    bool in_server = false;
    vhost_entry_t cur;
    memset(&cur, 0, sizeof(cur));
    strcpy(cur.index, "index.html"); // default

    uint32_t off = 0;
    char line[256];

    while (read_line((const char*)buf, sz, &off, line, sizeof(line))) {
        char* s = line;
        trim_left(&s);
        trim_right(s);

        if (!s[0]) continue;
        if (s[0] == '#') continue;
        if (starts_with(s, "//")) continue;

        if (!in_server) {
            // server {
            if (starts_with(s, "server")) {
                // accept: "server {" or "server{"
                if (strchr(s, '{')) {
                    in_server = true;
                    memset(&cur, 0, sizeof(cur));
                    strcpy(cur.index, "index.html");
                    cur.autoindex = false;
                }
            }
            continue;
        }

        // inside server block
        if (strchr(s, '}')) {
            // finalize entry
            if (cur.host[0] && cur.root[0]) {
                if (out->count < VHOSTS_MAX) {
                    out->items[out->count++] = cur;
                }
            }
            in_server = false;
            continue;
        }

        // key value;
        if (starts_with(s, "host")) {
            char* v = s + 4;
            trim_left(&v);
            copy_token(cur.host, VHOST_HOST_MAX, v);
            continue;
        }
        if (starts_with(s, "root")) {
            char* v = s + 4;
            trim_left(&v);
            copy_token(cur.root, VHOST_PATH_MAX, v);
            continue;
        }
        if (starts_with(s, "index")) {
            char* v = s + 5;
            trim_left(&v);
            copy_token(cur.index, VHOST_INDEX_MAX, v);
            continue;
        }
        if (starts_with(s, "autoindex")) {
            char* v = s + 9;
            trim_left(&v);
            // accept "on"/"off"
            char tok[32];
            copy_token(tok, sizeof(tok), v);
            cur.autoindex = (strcmp(tok, "on") == 0 || strcmp(tok, "1") == 0 || strcmp(tok, "true") == 0);
            continue;
        }
    }

    vfs_free_alloc(buf);
    printk("[vhosts] loaded %d entries\n", out->count);
    return true;
}