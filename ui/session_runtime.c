#include <ui/session_runtime.h>

#include <kernel/fs/vfs.h>
#include <kernel/printk.h>
#include <lib/string.h>
#include <stdint.h>

static int g_shell_running = 0;
static char g_shell_path[256];

static const char* skip_ws(const char* p) {
    while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') p++;
    return p;
}

static int read_line_value(const char* text, const char* key, char* out, int out_sz) {
    const char* p;
    const char* eq;
    int i = 0;

    if (!text || !key || !out || out_sz <= 0) return 0;

    p = strstr(text, key);
    if (!p) return 0;

    eq = strchr(p, '=');
    if (!eq) return 0;

    p = eq + 1;
    p = skip_ws(p);

    while (*p && *p != '\r' && *p != '\n' && i < out_sz - 1) {
        out[i++] = *p++;
    }
    out[i] = '\0';

    return (i > 0);
}

int session_run_path(const char* desktop_json_path) {
    uint8_t tmp[1];
    uint32_t out_sz = 0;

    if (!desktop_json_path || !desktop_json_path[0]) {
        printk("[session] invalid path\n");
        g_shell_running = 0;
        g_shell_path[0] = '\0';
        return 0;
    }

    if (!vfs_read_all(desktop_json_path, tmp, sizeof(tmp), &out_sz)) {
        printk("[session] file not found: %s\n", desktop_json_path);
        g_shell_running = 0;
        g_shell_path[0] = '\0';
        return 0;
    }

    strncpy(g_shell_path, desktop_json_path, sizeof(g_shell_path) - 1);
    g_shell_path[sizeof(g_shell_path) - 1] = '\0';
    g_shell_running = 1;

    printk("[session] shell started: %s\n", g_shell_path);
    return 1;
}

int session_kill_active(void) {
    if (!g_shell_running) {
        printk("[session] no active shell\n");
        return 0;
    }

    g_shell_running = 0;
    g_shell_path[0] = '\0';

    printk("[session] shell stopped\n");
    return 1;
}

int session_autorun_from_config(void) {
    char buf[512];
    uint32_t out_sz = 0;
    char desktop_path[256];

    memset(buf, 0, sizeof(buf));
    memset(desktop_path, 0, sizeof(desktop_path));

    if (!vfs_read_all("/etc/session.conf", (uint8_t*)buf, sizeof(buf) - 1, &out_sz)) {
        printk("[session] no /etc/session.conf\n");
        return 0;
    }

    buf[out_sz] = '\0';

    if (!read_line_value(buf, "desktop", desktop_path, sizeof(desktop_path))) {
        printk("[session] desktop= not found in /etc/session.conf\n");
        return 0;
    }

    printk("[session] autorun desktop=%s\n", desktop_path);
    return session_run_path(desktop_path);
}

const char* session_active_path(void) {
    return g_shell_path;
}

int session_is_shell_running(void) {
    return g_shell_running;
}