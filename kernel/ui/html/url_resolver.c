// ui/html/url_resolver.c
#include <ui/html/url_resolver.h>

#include <kernel/fs/vfs.h>
#include <kernel/printk.h>
#include <kernel/user.h>     // USER_DESKTOP_PATH

#include <lib/string.h>
#include <stdint.h>
#include <stdbool.h>

#ifndef URLR_MAX_HOST
#define URLR_MAX_HOST 64
#endif

#ifndef URLR_MAX_ROOT
#define URLR_MAX_ROOT 192
#endif

#ifndef URLR_MAX_ENTRIES
#define URLR_MAX_ENTRIES 64
#endif

#define URLR_CONF_PATH "/etc/vhosts.conf"

// ------------------------------------------------------------
// internal table
// ------------------------------------------------------------
typedef struct {
    char host[URLR_MAX_HOST];
    char root[URLR_MAX_ROOT];   // file or dir
    uint32_t ip;                // optional (0 if unused)
    bool used;
} urlr_entry_t;

static urlr_entry_t g_tab[URLR_MAX_ENTRIES];
static int g_loaded = 0;

// ------------------------------------------------------------
// small utils
// ------------------------------------------------------------
static bool is_space(char c) {
    return (c == ' ' || c == '\t' || c == '\r' || c == '\n');
}

static char* ltrim(char* s) {
    while (s && *s && is_space(*s)) s++;
    return s;
}

static void rtrim_inplace(char* s) {
    if (!s) return;
    int n = (int)strlen(s);
    while (n > 0 && is_space(s[n - 1])) {
        s[n - 1] = 0;
        n--;
    }
}

static void strip_comment_inplace(char* s) {
    if (!s) return;
    for (int i = 0; s[i]; i++) {
        if (s[i] == '#' || s[i] == ';') {
            s[i] = 0;
            break;
        }
    }
}

static bool safe_copy(char* out, int cap, const char* in) {
    if (!out || cap <= 0) return false;
    out[0] = 0;
    if (!in) return true;
    strncpy(out, in, (size_t)cap - 1);
    out[cap - 1] = 0;
    return true;
}

static bool safe_cat(char* out, int cap, const char* add) {
    if (!out || cap <= 0) return false;
    if (!add || !add[0]) return true;
    int ol = (int)strlen(out);
    int al = (int)strlen(add);
    if (ol + al >= cap) return false;
    strcat(out, add);
    return true;
}

static bool has_dot_ext_after_slash(const char* path) {
    // last segment has '.' ? (file-like)
    if (!path) return false;
    const char* base = strrchr(path, '/');
    base = base ? base + 1 : path;
    const char* dot = strrchr(base, '.');
    return (dot && dot != base && dot[1] != 0);
}

static bool looks_like_dir_root(const char* root) {
    if (!root || !root[0]) return false;
    int n = (int)strlen(root);
    if (root[n - 1] == '/') return true;
    // if no extension in last segment -> treat as dir
    return !has_dot_ext_after_slash(root);
}

static int is_abs_path(const char* s) {
    return (s && s[0] == '/');
}

// host + request-path split
static void split_url(const char* url,
                      char* out_host, int host_cap,
                      char* out_reqpath, int path_cap)
{
    if (out_host && host_cap > 0) out_host[0] = 0;
    if (out_reqpath && path_cap > 0) out_reqpath[0] = 0;
    if (!url || !url[0]) return;

    const char* s = url;

    // scheme strip: "http://"
    const char* p = strstr(s, "://");
    if (p) s = p + 3;

    // host ends at '/' or end
    const char* slash = strchr(s, '/');
    if (!slash) {
        safe_copy(out_host, host_cap, s);
        return;
    }

    int hl = (int)(slash - s);
    if (hl >= host_cap) hl = host_cap - 1;
    memcpy(out_host, s, (size_t)hl);
    out_host[hl] = 0;

    safe_copy(out_reqpath, path_cap, slash); // includes leading '/'
}

// basic token parser (space separated)
static int next_token(char** ps, char* out, int cap) {
    if (!ps || !*ps || !out || cap <= 0) return 0;

    char* s = *ps;
    s = ltrim(s);
    if (!s || !*s) { *ps = s; return 0; }

    int i = 0;
    while (s[i] && !is_space(s[i])) {
        if (i < cap - 1) out[i] = s[i];
        i++;
    }
    out[(i < cap) ? i : (cap - 1)] = 0;

    // advance
    while (s[i] && !is_space(s[i])) i++;
    *ps = s + i;
    return 1;
}

static uint32_t parse_ipv4(const char* s) {
    // very small: a.b.c.d (0..255)
    if (!s) return 0;
    int parts[4] = {0,0,0,0};
    int pi = 0;
    int val = 0;
    int have = 0;

    for (int i=0; s[i]; i++) {
        char ch = s[i];
        if (ch >= '0' && ch <= '9') {
            val = val*10 + (ch - '0');
            if (val > 255) return 0;
            have = 1;
        } else if (ch == '.') {
            if (!have) return 0;
            if (pi >= 4) return 0;
            parts[pi++] = val;
            val = 0;
            have = 0;
        } else {
            return 0;
        }
    }
    if (!have) return 0;
    if (pi != 3) return 0;
    parts[pi] = val;

    return ((uint32_t)parts[0]<<24) | ((uint32_t)parts[1]<<16) | ((uint32_t)parts[2]<<8) | (uint32_t)parts[3];
}

static void table_clear(void) {
    memset(g_tab, 0, sizeof(g_tab));
}

static bool table_add(const char* host, const char* root, uint32_t ip) {
    if (!host || !host[0] || !root || !root[0]) return false;

    // overwrite if exists
    for (int i=0;i<URLR_MAX_ENTRIES;i++) {
        if (g_tab[i].used && strcmp(g_tab[i].host, host) == 0) {
            safe_copy(g_tab[i].root, URLR_MAX_ROOT, root);
            g_tab[i].ip = ip;
            return true;
        }
    }

    for (int i=0;i<URLR_MAX_ENTRIES;i++) {
        if (!g_tab[i].used) {
            g_tab[i].used = true;
            safe_copy(g_tab[i].host, URLR_MAX_HOST, host);
            safe_copy(g_tab[i].root, URLR_MAX_ROOT, root);
            g_tab[i].ip = ip;
            return true;
        }
    }
    return false;
}

static const urlr_entry_t* table_find(const char* host) {
    if (!host || !host[0]) return NULL;
    for (int i=0;i<URLR_MAX_ENTRIES;i++) {
        if (g_tab[i].used && strcmp(g_tab[i].host, host) == 0) return &g_tab[i];
    }
    return NULL;
}

static bool vfs_file_exists_ro(const char* path) {
    if (!path || !path[0]) return false;
    vfs_file_t* f = 0;
    if (vfs_open(path, VFS_O_RDONLY, &f) == 1) {
        vfs_close(f);
        return true;
    }
    return false;
}

// ------------------------------------------------------------
// load config
// ------------------------------------------------------------
static void parse_line(char* line) {
    if (!line) return;

    strip_comment_inplace(line);
    rtrim_inplace(line);
    char* s = ltrim(line);
    if (!s || !*s) return;

    // support "host=path"
    char* eq = strchr(s, '=');
    if (eq) {
        *eq = 0;
        char* host = ltrim(s);
        rtrim_inplace(host);

        char* root = ltrim(eq + 1);
        rtrim_inplace(root);

        if (host[0] && root[0]) {
            table_add(host, root, 0);
        }
        return;
    }

    // token forms:
    // (A) ip host root
    // (B) host root
    char t1[URLR_MAX_ROOT];
    char t2[URLR_MAX_ROOT];
    char t3[URLR_MAX_ROOT];

    char* ps = s;
    if (!next_token(&ps, t1, (int)sizeof(t1))) return;
    if (!next_token(&ps, t2, (int)sizeof(t2))) return;

    // third token optional
    int has3 = next_token(&ps, t3, (int)sizeof(t3));

    uint32_t ip = parse_ipv4(t1);
    if (ip != 0) {
        const char* host = t2;
        const char* root = has3 ? t3 : "";
        if (root[0]) table_add(host, root, ip);
        return;
    }

    // host root
    table_add(t1, t2, 0);
}

static void ensure_loaded(void) {
    if (g_loaded) return;
    g_loaded = 1;

    table_clear();

    uint8_t* buf = 0;
    uint32_t sz = 0;
    if (!vfs_read_all_alloc(URLR_CONF_PATH, &buf, &sz)) {
        printk("[url_resolver] no %s (skip)\n", URLR_CONF_PATH);
        return;
    }

    // parse line by line (in-place)
    char* text = (char*)buf;
    uint32_t i = 0;
    uint32_t start = 0;

    while (i <= sz) {
        char ch = (i < sz) ? text[i] : '\n';
        if (ch == '\n' || ch == '\0') {
            text[i] = 0;
            parse_line(&text[start]);
            start = i + 1;
        }
        i++;
    }

    vfs_free_alloc(buf);
    printk("[url_resolver] loaded %s\n", URLR_CONF_PATH);
}

// ------------------------------------------------------------
// local: mapping
// ------------------------------------------------------------
static bool resolve_local(const char* url, char* out, int cap) {
    if (!url || strncmp(url, "local:", 6) != 0) return false;

    const char* p = url + 6;

    // local:   -> home
    if (!p[0] || strcmp(p, "home") == 0) {
        safe_copy(out, cap, USER_DESKTOP_PATH "/home.html");
        return true;
    }
    if (strcmp(p, "docs") == 0) {
        safe_copy(out, cap, USER_DESKTOP_PATH "/docs.html");
        return true;
    }
    if (strcmp(p, "new") == 0) {
        safe_copy(out, cap, USER_DESKTOP_PATH "/new.html");
        return true;
    }

    // İstersen local:foo.html => desktop/foo.html yap:
    // safe_copy(out, cap, USER_DESKTOP_PATH "/");
    // safe_cat(out, cap, p);
    // return true;

    return false;
}

// ------------------------------------------------------------
// public API
// ------------------------------------------------------------
bool url_resolve_to_path(const char* url, char* out, int cap) {
    if (!out || cap <= 0) return false;
    out[0] = 0;

    ensure_loaded();

    // empty => default home
    if (!url || !url[0]) {
        safe_copy(out, cap, USER_DESKTOP_PATH "/home.html");
        return true;
    }

    // local:
    if (resolve_local(url, out, cap)) return true;

    // absolute path
    if (is_abs_path(url)) {
        safe_copy(out, cap, url);
        return true;
    }

    // split host + req
    char host[URLR_MAX_HOST];
    char req[128];
    split_url(url, host, (int)sizeof(host), req, (int)sizeof(req));

    if (!host[0]) return false;

    const urlr_entry_t* e = table_find(host);
    if (!e) return false;

    char tmp[256];
    tmp[0] = 0;

    const bool root_is_dir = looks_like_dir_root(e->root);

    if (root_is_dir) {
        // base root
        safe_copy(tmp, (int)sizeof(tmp), e->root);

        // ensure trailing slash on root when joining
        int tl = (int)strlen(tmp);
        if (tl > 0 && tmp[tl - 1] != '/') safe_cat(tmp, (int)sizeof(tmp), "/");

        if (!req[0] || strcmp(req, "/") == 0) {
            // default index.html
            safe_cat(tmp, (int)sizeof(tmp), "index.html");
            if (!vfs_file_exists_ro(tmp)) return false;
        } else {
            // req starts with '/', append without leading '/'
            const char* rp = (req[0] == '/') ? (req + 1) : req;
            safe_cat(tmp, (int)sizeof(tmp), rp);

            // If it doesn't exist, try "req/index.html" (apache-like)
            if (!vfs_file_exists_ro(tmp)) {
                // tmp currently ".../about" -> try ".../about/index.html"
                safe_cat(tmp, (int)sizeof(tmp), "/index.html");
                if (!vfs_file_exists_ro(tmp)) return false;
            }
        }
    } else {
        // mapped to a file (e->root)
        if (!req[0] || strcmp(req, "/") == 0) {
            safe_copy(tmp, (int)sizeof(tmp), e->root);
            if (!vfs_file_exists_ro(tmp)) return false;
        } else {
            // interpret req relative to file's directory
            safe_copy(tmp, (int)sizeof(tmp), e->root);

            // cut to directory
            char* last = strrchr(tmp, '/');
            if (last) *(last + 1) = 0;

            const char* rp = (req[0] == '/') ? (req + 1) : req;
            safe_cat(tmp, (int)sizeof(tmp), rp);

            if (!vfs_file_exists_ro(tmp)) return false;
        }
    }

    safe_copy(out, cap, tmp);
    return true;
}