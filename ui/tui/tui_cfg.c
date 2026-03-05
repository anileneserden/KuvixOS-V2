#include <ui/tui/tui_cfg.h>
#include <ui/tui/tui.h>

#include <kernel/fs/vfs.h>
#include <kernel/memory/kmalloc.h>

#include <lib/string.h>
#include <stdint.h>

#define TUI_CFG_MAX (16 * 1024)

// ------------------------------------------------------------
// küçük string helper'lar (trim, split)
// ------------------------------------------------------------
static int is_space(char c) {
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

static void trim_inplace(char* s) {
    if (!s) return;
    // left trim -> memmove
    char* p = ltrim(s);
    if (p != s) memmove(s, p, strlen(p) + 1);
    rtrim_inplace(s);
}

static char* str_find_char(char* s, char ch) {
    if (!s) return 0;
    while (*s) { if (*s == ch) return s; s++; }
    return 0;
}

static int starts_with(const char* s, const char* pref) {
    if (!s || !pref) return 0;
    while (*pref) {
        if (*s++ != *pref++) return 0;
    }
    return 1;
}

// ------------------------------------------------------------
// Parser
// Format:
//   title=KuvixOS
//   item=Terminal|session:tty1
//   item=Desktop|session:desktop
// comments: # ... or ; ...
// ------------------------------------------------------------
int tui_load_cfg_text(const char* text) {
    tui_clear();
    if (!text) return 0;

    // text immutable olabilir -> kopyala
    int len = (int)strlen(text);
    if (len <= 0) return 0;

    char* buf = (char*)kmalloc((uint32_t)len + 1);
    if (!buf) return 0;

    memcpy(buf, text, (uint32_t)len);
    buf[len] = 0;

    char* p = buf;

    while (*p) {
        // satır al
        char* line = p;
        while (*p && *p != '\n') p++;
        if (*p == '\n') { *p = 0; p++; }

        trim_inplace(line);
        if (line[0] == 0) continue;

        // comment
        if (line[0] == '#' || line[0] == ';') continue;

        // inline comment (# veya ;)
        for (int i = 0; line[i]; i++) {
            if (line[i] == '#' || line[i] == ';') { line[i] = 0; break; }
        }
        trim_inplace(line);
        if (line[0] == 0) continue;

        // key=value
        char* eq = str_find_char(line, '=');
        if (!eq) continue;

        *eq = 0;
        char* key = line;
        char* val = eq + 1;

        trim_inplace(key);
        trim_inplace(val);

        if (key[0] == 0) continue;

        if (starts_with(key, "title")) {
            if (val[0]) tui_set_title(val);
            continue;
        }

        if (starts_with(key, "item")) {
            // val = Label|action
            char* bar = str_find_char(val, '|');
            if (!bar) continue;
            *bar = 0;

            char* label = val;
            char* action = bar + 1;

            trim_inplace(label);
            trim_inplace(action);

            if (label[0] && action[0]) {
                tui_add_item(label, action);
            }
            continue;
        }

        // istersen ileride:
        // bg=0x00202020
        // fg=0x00FFFFFF
    }

    kfree(buf);

    // item yoksa fail
    if (tui_get_item_count() <= 0) return 0;

    tui_init();   // ✅ çiz
    return 1;
}

int tui_load_cfg(const char* path) {
    uint8_t* data = 0;
    uint32_t sz = 0;

    uint32_t cap = 8192;
    data = (uint8_t*)kmalloc(cap);
    if (!data) return 0;

    // ✅ junk kalmasın
    memset(data, 0, cap);

    // ✅ cap-1 oku ki NUL için yer kalsın
    if (!vfs_read_all(path, data, cap - 1, &sz)) {
        kfree(data);
        return 0;
    }

    // ✅ asıl kritik: NUL'ü okunan byte'ın sonuna koy
    if (sz >= cap) sz = cap - 1;
    data[sz] = 0;

    int ok = tui_load_cfg_text((const char*)data);

    kfree(data);
    return ok;
}