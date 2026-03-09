// kernel/user.c
#include <kernel/user.h>

#include <kernel/fs/vfs.h>
#include <kernel/printk.h>
#include <lib/string.h>

static user_profile_t g_user;

/* ------------------------------------------------------------ */
/* helpers                                                      */
/* ------------------------------------------------------------ */

static bool starts_with(const char* s, const char* pre) {
    if (!s || !pre) return false;
    while (*pre) {
        if (*s++ != *pre++) return false;
    }
    return true;
}

static void str_append(char* out, int out_sz, const char* s) {
    if (!out || out_sz <= 0 || !s) return;

    int cur = (int)strlen(out);
    int i = 0;

    while (s[i] && (cur + i) < out_sz - 1) {
        out[cur + i] = s[i];
        i++;
    }

    out[cur + i] = '\0';
}

static void safe_copy(char* dst, int dst_sz, const char* src) {
    if (!dst || dst_sz <= 0) return;
    if (!src) {
        dst[0] = '\0';
        return;
    }

    strncpy(dst, src, (size_t)(dst_sz - 1));
    dst[dst_sz - 1] = '\0';
}

static void trim(char* s) {
    if (!s || !s[0]) return;

    int len = (int)strlen(s);
    int start = 0;

    while (s[start] == ' ' || s[start] == '\t' || s[start] == '\r' || s[start] == '\n') {
        start++;
    }

    int end = len - 1;
    while (end >= start &&
           (s[end] == ' ' || s[end] == '\t' || s[end] == '\r' || s[end] == '\n')) {
        end--;
    }

    int j = 0;
    for (int i = start; i <= end; i++) {
        s[j++] = s[i];
    }
    s[j] = '\0';
}

static bool split_key_value(char* line, char** out_key, char** out_val) {
    if (!line || !out_key || !out_val) return false;

    char* eq = NULL;
    for (char* p = line; *p; ++p) {
        if (*p == '=') {
            eq = p;
            break;
        }
    }

    if (!eq) return false;

    *eq = '\0';
    *out_key = line;
    *out_val = eq + 1;

    trim(*out_key);
    trim(*out_val);

    return ((*out_key)[0] != '\0');
}

/* ------------------------------------------------------------ */
/* defaults / load                                              */
/* ------------------------------------------------------------ */

void user_set_defaults(void) {
    safe_copy(g_user.username, sizeof(g_user.username), "anil");
    safe_copy(g_user.hostname, sizeof(g_user.hostname), "kuvixos");
    safe_copy(g_user.home, sizeof(g_user.home), "/home/anil");
}

bool user_load(const char* path) {
    if (!path) return false;

    uint8_t buf[512];
    uint32_t out_sz = 0;

    int r = vfs_read_all(path, buf, sizeof(buf) - 1, &out_sz);
    if (r != 1) {
        printk("[user] read failed: %s\n", path);
        return false;
    }

    buf[out_sz] = 0;

    char* p = (char*)buf;
    while (*p) {
        char line[256];
        int li = 0;

        while (*p && *p != '\n' && li < (int)sizeof(line) - 1) {
            line[li++] = *p++;
        }
        line[li] = '\0';

        if (*p == '\n') p++;

        trim(line);
        if (!line[0]) continue;
        if (line[0] == '#') continue;

        char* key = NULL;
        char* val = NULL;
        if (!split_key_value(line, &key, &val)) continue;

        if (strcmp(key, "username") == 0) {
            safe_copy(g_user.username, sizeof(g_user.username), val);
        } else if (strcmp(key, "hostname") == 0) {
            safe_copy(g_user.hostname, sizeof(g_user.hostname), val);
        } else if (strcmp(key, "home") == 0) {
            safe_copy(g_user.home, sizeof(g_user.home), val);
        }
    }

    printk("[user] loaded: username=%s hostname=%s home=%s\n",
           g_user.username, g_user.hostname, g_user.home);

    return true;
}

/* ------------------------------------------------------------ */
/* getters                                                      */
/* ------------------------------------------------------------ */

const char* user_get_username(void) {
    return g_user.username;
}

const char* user_get_hostname(void) {
    return g_user.hostname;
}

const char* user_get_home(void) {
    return g_user.home;
}

void user_get_desktop_path(char* out, int out_sz) {
    if (!out || out_sz <= 0) return;
    out[0] = '\0';

    str_append(out, out_sz, g_user.home);
    str_append(out, out_sz, "/desktop");
}

void user_get_trash_path(char* out, int out_sz) {
    if (!out || out_sz <= 0) return;
    out[0] = '\0';

    str_append(out, out_sz, g_user.home);
    str_append(out, out_sz, "/trash");
}

void user_get_apps_path(char* out, int out_sz) {
    if (!out || out_sz <= 0) return;
    out[0] = '\0';

    str_append(out, out_sz, g_user.home);
    str_append(out, out_sz, "/apps");
}

/* ------------------------------------------------------------ */
/* formatting                                                   */
/* ------------------------------------------------------------ */

void user_format_path(const char* abs_path, char* out, int out_sz, user_lang_t lang) {
    if (!out || out_sz <= 0) return;
    out[0] = '\0';

    if (!abs_path || !abs_path[0]) {
        str_append(out, out_sz, "~");
        return;
    }

    char desktop[160];
    char trash[160];
    char apps[160];

    user_get_desktop_path(desktop, sizeof(desktop));
    user_get_trash_path(trash, sizeof(trash));
    user_get_apps_path(apps, sizeof(apps));

    if (starts_with(abs_path, desktop)) {
        str_append(out, out_sz, "~/");
        str_append(out, out_sz, (lang == USER_LANG_TR) ? "Masaustu" : "Desktop");

        const char* rest = abs_path + strlen(desktop);
        if (rest[0] == '/') str_append(out, out_sz, rest);
        return;
    }

    if (starts_with(abs_path, trash)) {
        str_append(out, out_sz, "~/");
        str_append(out, out_sz, (lang == USER_LANG_TR) ? "CopKutusu" : "Trash");

        const char* rest = abs_path + strlen(trash);
        if (rest[0] == '/') str_append(out, out_sz, rest);
        return;
    }

    if (starts_with(abs_path, apps)) {
        str_append(out, out_sz, "~/apps");
        const char* rest = abs_path + strlen(apps);
        if (rest[0] == '/') str_append(out, out_sz, rest);
        return;
    }

    if (starts_with(abs_path, g_user.home)) {
        str_append(out, out_sz, "~");
        const char* rest = abs_path + strlen(g_user.home);
        if (rest[0]) str_append(out, out_sz, rest);
        return;
    }

    str_append(out, out_sz, abs_path);
}

void user_format_prompt(const char* cwd_abs, char* out, int out_sz, user_lang_t lang) {
    if (!out || out_sz <= 0) return;
    out[0] = '\0';

    str_append(out, out_sz, user_get_username());
    str_append(out, out_sz, "@");
    str_append(out, out_sz, user_get_hostname());
    str_append(out, out_sz, ":");

    char pbuf[256];
    user_format_path(cwd_abs, pbuf, sizeof(pbuf), lang);
    str_append(out, out_sz, pbuf);
}