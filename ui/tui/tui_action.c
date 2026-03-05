// ui/tui/tui_action.c
#include <ui/tui/tui_action.h>
#include <ui/tui/tui_cfg.h>
#include <ui/tui/tui_input.h>
#include <ui/tui/tui.h>

#include <ui/session.h>

#include <kernel/power.h>
#include <kernel/printk.h>
#include <lib/commands.h>
#include <lib/string.h>

static int starts_with(const char* s, const char* p) {
    if (!s || !p) return 0;
    while (*p) {
        if (*s++ != *p++) return 0;
    }
    return 1;
}

static const char* skip_ws(const char* s) {
    while (s && (*s == ' ' || *s == '\t' || *s == '\r' || *s == '\n')) s++;
    return s;
}

static void rstrip_inplace(char* s) {
    if (!s) return;
    int n = (int)strlen(s);
    while (n > 0) {
        char c = s[n - 1];
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
            s[n - 1] = 0;
            n--;
        } else {
            break;
        }
    }
}

static void lstrip_inplace(char* s) {
    if (!s) return;
    int i = 0;
    while (s[i] == ' ' || s[i] == '\t' || s[i] == '\r' || s[i] == '\n') i++;
    if (i > 0) {
        int j = 0;
        while (s[i]) s[j++] = s[i++];
        s[j] = 0;
    }
}

static void trim_inplace(char* s) {
    lstrip_inplace(s);
    rstrip_inplace(s);
}

// virgülle 4 parça: a,b,c,d
static void parse_csv4(const char* s,
                       char* a, int ac,
                       char* b, int bc,
                       char* c, int cc,
                       char* d, int dc) {
    if (a && ac) a[0] = 0;
    if (b && bc) b[0] = 0;
    if (c && cc) c[0] = 0;
    if (d && dc) d[0] = 0;

    int part = 0;
    char* out = a;
    int cap = ac;
    int o = 0;

    while (s && *s) {
        char ch = *s++;

        if (ch == ',') {
            if (out && cap > 0) out[(o < cap) ? o : (cap - 1)] = 0;

            part++;
            o = 0;

            if (part == 1) { out = b; cap = bc; }
            else if (part == 2) { out = c; cap = cc; }
            else if (part == 3) { out = d; cap = dc; }
            else { out = 0; cap = 0; }
            continue;
        }

        if (out && cap > 1 && o < cap - 1) {
            out[o++] = ch;
        } else {
            o++; // taşsa bile say
        }
    }

    if (out && cap > 0) out[(o < cap) ? o : (cap - 1)] = 0;

    // trimle (çok önemli: \n vs. kalmasın)
    if (a && ac) { a[ac - 1] = 0; trim_inplace(a); }
    if (b && bc) { b[bc - 1] = 0; trim_inplace(b); }
    if (c && cc) { c[cc - 1] = 0; trim_inplace(c); }
    if (d && dc) { d[dc - 1] = 0; trim_inplace(d); }
}

void tui_execute_action(const char* action) {
    if (!action) return;

    action = skip_ws(action);
    if (!action[0]) return;

    // ----------------------------
    // cmd: -> commands_execute()
    // ----------------------------
    if (starts_with(action, "cmd:")) {
        const char* cmd = skip_ws(action + 4);
        if (cmd && cmd[0]) {
            commands_execute(cmd);
        }
        return;
    }

    // ----------------------------
    // cfg: -> başka menü yükle
    // ----------------------------
    if (starts_with(action, "cfg:")) {
        const char* p = skip_ws(action + 4);

        // path'i güvenli buffer'a alıp trimle (satır sonu vs.)
        char path[128];
        path[0] = 0;
        if (p) {
            strncpy(path, p, sizeof(path) - 1);
            path[sizeof(path) - 1] = 0;
            trim_inplace(path);
        }

        if (!path[0]) {
            printk("[TUI] cfg: empty path\n");
            return;
        }

        tui_clear();
        tui_load_cfg(path);
        return;
    }

    // ----------------------------
    // session:
    // ----------------------------
    if (starts_with(action, "session:")) {
        const char* p = skip_ws(action + 8);

        char s[32];
        s[0] = 0;
        if (p) {
            strncpy(s, p, sizeof(s) - 1);
            s[sizeof(s) - 1] = 0;
            trim_inplace(s);
        }

        if (starts_with(s, "tty1"))        ui_session_switch(UI_SESSION_TTY1);
        else if (starts_with(s, "desktop")) ui_session_switch(UI_SESSION_DESKTOP);
        else if (starts_with(s, "tui"))     ui_session_switch(UI_SESSION_TUI);
        else printk("[TUI] unknown session: %s\n", s);

        return;
    }

    // ----------------------------
    // sys:
    // ----------------------------
    if (starts_with(action, "sys:")) {
        const char* p = skip_ws(action + 4);

        char s[32];
        s[0] = 0;
        if (p) {
            strncpy(s, p, sizeof(s) - 1);
            s[sizeof(s) - 1] = 0;
            trim_inplace(s);
        }

        if (starts_with(s, "reboot")) {
            power_reboot();
        } else if (starts_with(s, "poweroff")) {
            power_shutdown();
        } else {
            printk("[TUI] unknown sys action: %s\n", s);
        }
        return;
    }

    // ----------------------------
    // input:path,key,next,title
    // ----------------------------
    if (starts_with(action, "input:")) {
        const char* p = skip_ws(action + 6);

        char path[128] = {0};
        char key[64]   = {0};
        char next[128] = {0};
        char title[64] = {0};

        parse_csv4(p, path, sizeof(path), key, sizeof(key), next, sizeof(next), title, sizeof(title));

        if (!title[0]) {
            strncpy(title, key[0] ? key : "Input", sizeof(title) - 1);
            title[sizeof(title) - 1] = 0;
        }

        if (!path[0] || !key[0]) {
            printk("[TUI] input: missing path/key\n");
            return;
        }

        tui_input_begin(title, path, key, next[0] ? next : NULL, NULL);
        return;
    }

    printk("[TUI] unknown action: %s\n", action);
}