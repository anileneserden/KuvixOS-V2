#include <lib/shell.h>

#include <kernel/printk.h>
#include <kernel/drivers/video/fb_console.h>

#include <lib/commands.h>
#include <lib/string.h>

#include <kernel/memory/kmalloc.h>
#include <kernel/fs/vfs.h>

#include <stdint.h>

/* senin decoder */
extern char kbd_scancode_to_ascii(uint8_t scancode);

/* prompt identity */
static char g_username[32] = "root";
static char g_hostname[32] = "kuvix";
static char g_cwd[128]     = "/home";

/* line state */
static char g_line[128];
static int  g_len = 0;

/* -------------------------------------------------
   ✅ HEREDOC STATE
------------------------------------------------- */
static int  g_heredoc = 0;
static char g_hd_path[256];
static char g_hd_end[16];

static char*    g_hd_buf = 0;
static uint32_t g_hd_len = 0;
static uint32_t g_hd_cap = 0;

/* -------------------------------------------------
   Helpers
------------------------------------------------- */
static void shell_print_prompt(void) {
    fb_console_set_color(0x0000FF00, 0x00000000);
    printk("%s", g_username);
    printk("@%s", g_hostname);
    printk(":%s", g_cwd);

    fb_console_set_color(0x00FFFFFF, 0x00000000);
    printk("$ ");
    fb_console_flush();
}

static void shell_cmd_out(void* u, const char* s) {
    (void)u;
    if (!s) return;
    printk("%s", s);
    fb_console_flush();
}

static void shell_cmd_clear(void* u) {
    (void)u;
    fb_console_clear();
    fb_console_flush();
}

/* ---------- heredoc helpers ---------- */
static void hd_prompt(void) {
    fb_console_set_color(0x00AAAAFF, 0x00000000);
    printk("> ");
    fb_console_set_color(0x00FFFFFF, 0x00000000);
    fb_console_flush();
}

static void hd_buf_reset(void) {
    if (!g_hd_buf) {
        g_hd_cap = 4096;
        g_hd_buf = (char*)kmalloc(g_hd_cap);
    }
    g_hd_len = 0;
    if (g_hd_buf) g_hd_buf[0] = 0;
}

static void hd_append_line(const char* s) {
    if (!g_hd_buf) return;

    uint32_t n = (uint32_t)strlen(s);

    /* + '\n' + '\0' */
    if (g_hd_len + n + 2 >= g_hd_cap) {
        uint32_t newcap = g_hd_cap ? (g_hd_cap * 2) : 4096;
        while (g_hd_len + n + 2 >= newcap) newcap *= 2;

        char* nb = (char*)kmalloc(newcap);
        if (!nb) return;

        memcpy(nb, g_hd_buf, g_hd_len);
        kfree(g_hd_buf);
        g_hd_buf = nb;
        g_hd_cap = newcap;
    }

    memcpy(g_hd_buf + g_hd_len, s, n);
    g_hd_len += n;
    g_hd_buf[g_hd_len++] = '\n';
    g_hd_buf[g_hd_len] = 0;
}

static void hd_finish(void) {
    if (!g_hd_buf) {
        printk("[HEREDOC] internal buffer missing\n");
        fb_console_flush();
        g_heredoc = 0;
        shell_print_prompt();
        return;
    }

    int ok = vfs_write_all(g_hd_path, (const uint8_t*)g_hd_buf, g_hd_len);
    if (ok) printk("[HEREDOC] saved %u bytes -> %s\n", (unsigned)g_hd_len, g_hd_path);
    else    printk("[HEREDOC] write failed -> %s\n", g_hd_path);

    fb_console_flush();

    g_heredoc = 0;
    g_hd_len = 0;

    /* normal prompt’a dön */
    shell_print_prompt();
}

/* -------------------------------------------------
   Public API
------------------------------------------------- */
void shell_set_username(const char* u) { if (u) { strncpy(g_username,u,sizeof(g_username)-1); g_username[31]=0; } }
void shell_set_hostname(const char* h) { if (h) { strncpy(g_hostname,h,sizeof(g_hostname)-1); g_hostname[31]=0; } }
void shell_set_cwd(const char* p)      { if (p) { strncpy(g_cwd,p,sizeof(g_cwd)-1); g_cwd[127]=0; } }

void shell_begin_heredoc(const char* path, const char* endtok) {
    if (!path || !path[0] || !endtok || !endtok[0]) {
        printk("Kullanim: heredoc <dosya> <EOF>\n");
        fb_console_flush();
        return;
    }

    strncpy(g_hd_path, path, sizeof(g_hd_path)-1);
    g_hd_path[sizeof(g_hd_path)-1] = 0;

    strncpy(g_hd_end, endtok, sizeof(g_hd_end)-1);
    g_hd_end[sizeof(g_hd_end)-1] = 0;

    g_heredoc = 1;
    hd_buf_reset();

    printk("[HEREDOC] writing to %s (end token: %s)\n", g_hd_path, g_hd_end);
    fb_console_flush();

    /* satır state reset */
    g_len = 0;
    g_line[0] = 0;

    hd_prompt();
}

void shell_init(void) {
    printk("KuvixOS Shell V2 Hazir!\n");
    printk("Komutlar icin 'help' yazabilirsiniz.\n");
    fb_console_flush();

    commands_set_output(shell_cmd_out, NULL);
    commands_set_clear(shell_cmd_clear, NULL);

    g_len = 0;
    g_line[0] = 0;

    shell_print_prompt();
}

void shell_tick(void) {
    /* cursor blink vs istersen buraya */
}

/* -------------------------------------------------
   Input handler
------------------------------------------------- */
void shell_handle_scancode(uint16_t ev) {
    uint8_t sc = (uint8_t)ev;

    /* break ignore */
    if (sc & 0x80) return;

    /* extended E0 xx -> şimdilik ignore */
    if ((ev & 0xFF00) != 0) return;

    char c = kbd_scancode_to_ascii(sc);
    if (!c) return;

    /* =====================================================
       ✅ HEREDOC MODE ÖNCE GELMELİ (kritik)
    ===================================================== */
    if (g_heredoc) {
        /* ENTER */
        if (c == '\n' || c == '\r') {
            g_line[g_len] = '\0';
            printk("\n");
            fb_console_flush();

            if (strcmp(g_line, g_hd_end) == 0) {
                g_len = 0;
                g_line[0] = 0;
                hd_finish();
                return;
            }

            hd_append_line(g_line);
            g_len = 0;
            g_line[0] = 0;

            hd_prompt();
            return;
        }

        /* BACKSPACE */
        if (c == '\b' || (uint8_t)c == 127) {
            if (g_len > 0) {
                g_len--;
                g_line[g_len] = '\0';
                printk("\b \b");
                fb_console_flush();
            }
            return;
        }

        /* TAB = 4 space */
        if (c == '\t') {
            for (int i = 0; i < 4; i++) {
                if (g_len < (int)sizeof(g_line) - 1) {
                    g_line[g_len++] = ' ';
                    g_line[g_len] = '\0';
                    printk(" ");
                }
            }
            fb_console_flush();
            return;
        }

        /* normal char */
        if ((uint8_t)c >= 32 && g_len < (int)sizeof(g_line) - 1) {
            g_line[g_len++] = c;
            g_line[g_len] = '\0';
            printk("%c", (unsigned char)c);
            fb_console_flush();
        }
        return;
    }

    /* =====================================================
       NORMAL SHELL MODE
    ===================================================== */

    /* ENTER */
    if (c == '\n' || c == '\r') {
        g_line[g_len] = '\0';
        printk("\n");
        fb_console_flush();

        if (g_len > 0) {
            commands_set_output(shell_cmd_out, NULL);
            commands_set_clear(shell_cmd_clear, NULL);
            commands_execute(g_line);
            fb_console_flush();
        }

        g_len = 0;
        g_line[0] = 0;

        if (!g_heredoc) {
            shell_print_prompt();
        }
        return;
    }

    /* BACKSPACE */
    if (c == '\b' || (uint8_t)c == 127) {
        if (g_len > 0) {
            g_len--;
            g_line[g_len] = '\0';
            printk("\b \b");
            fb_console_flush();
        }
        return;
    }

    /* TAB -> 4 space */
    if (c == '\t') {
        for (int i = 0; i < 4; i++) {
            if (g_len < (int)sizeof(g_line) - 1) {
                g_line[g_len++] = ' ';
                g_line[g_len] = '\0';
                printk(" ");
            }
        }
        fb_console_flush();
        return;
    }

    /* normal */
    if ((uint8_t)c >= 32 && g_len < (int)sizeof(g_line) - 1) {
        g_line[g_len++] = c;
        g_line[g_len] = '\0';
        printk("%c", (unsigned char)c);
        fb_console_flush();
    }
}