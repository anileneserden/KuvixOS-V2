// kernel/user.c
#include <kernel/user.h>
#include <lib/string.h>
#include <lib/commands.h>

// küçük yardımcı: prefix match
static bool starts_with(const char* s, const char* pre) {
    if (!s || !pre) return false;
    while (*pre) {
        if (*s++ != *pre++) return false;
    }
    return true;
}

// küçük yardımcı: güvenli append
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

// "/persist/home/anil/..." -> "/home/anil/..."
// diğer path'leri olduğu gibi bırakır
static const char* normalize_user_visible_path(const char* abs_path) {
    if (!abs_path) return abs_path;

    if (starts_with(abs_path, "/persist/home/")) {
        return abs_path + 8; // "/persist" kısmını atla
    }

    return abs_path;
}

void user_format_path(const char* abs_path, char* out, int out_sz, user_lang_t lang) {
    if (!out || out_sz <= 0) return;
    out[0] = '\0';

    if (!abs_path || !abs_path[0]) {
        str_append(out, out_sz, "~");
        return;
    }

    // Önce kullanıcıya gösterilecek mantıksal path'e normalize et
    abs_path = normalize_user_visible_path(abs_path);

    // 1) Desktop özel eşleşme (en spesifik önce)
    if (starts_with(abs_path, USER_DESKTOP_PATH)) {
        str_append(out, out_sz, "~/");
        str_append(out, out_sz, (lang == USER_LANG_TR) ? "Masaustu" : "Desktop");

        const char* rest = abs_path + strlen(USER_DESKTOP_PATH);
        if (rest[0] == '/') str_append(out, out_sz, rest);
        return;
    }

    // 2) Trash
    if (starts_with(abs_path, USER_TRASH_PATH)) {
        str_append(out, out_sz, "~/");
        str_append(out, out_sz, (lang == USER_LANG_TR) ? "CopKutusu" : "Trash");

        const char* rest = abs_path + strlen(USER_TRASH_PATH);
        if (rest[0] == '/') str_append(out, out_sz, rest);
        return;
    }

    // 3) Apps
    if (starts_with(abs_path, USER_APPS_PATH)) {
        str_append(out, out_sz, "~/apps");
        const char* rest = abs_path + strlen(USER_APPS_PATH);
        if (rest[0] == '/') str_append(out, out_sz, rest);
        return;
    }

    // 4) Home geneli: "/home/anil/xxx" -> "~/xxx"
    if (starts_with(abs_path, USER_HOME_PATH)) {
        str_append(out, out_sz, "~");
        const char* rest = abs_path + strlen(USER_HOME_PATH);
        if (rest[0]) str_append(out, out_sz, rest);
        return;
    }

    // 5) Home değilse aynen bas
    str_append(out, out_sz, abs_path);
}

void user_format_prompt(const char* cwd_abs, char* out, int out_sz, user_lang_t lang) {
    if (!out || out_sz <= 0) return;
    out[0] = '\0';

    // user@host:
    str_append(out, out_sz, CURRENT_USER);
    str_append(out, out_sz, "@");
    str_append(out, out_sz, HOST);
    str_append(out, out_sz, ":");

    // pretty cwd
    char pbuf[256];
    user_format_path(cwd_abs, pbuf, sizeof(pbuf), lang);
    str_append(out, out_sz, pbuf);

    str_append(out, out_sz, " ");
}