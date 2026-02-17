#include <lib/shell.h>
#include <kernel/printk.h>
#include <kernel/kbd.h>
#include <lib/commands.h>
#include <kernel/drivers/video/fb_console.h>
#include <kernel/drivers/input/keyboard.h>
#include <lib/string.h>
#include <stdint.h>

static char g_username[32] = "root";
static char g_hostname[32] = "kuvix";
static char g_cwd[128]     = "/home";

/* line state */
static char g_line[128];
static int  g_len = 0;
static int  g_prompt = 0;

void shell_set_username(const char* u) { if (u) { strncpy(g_username,u,sizeof(g_username)-1); g_username[31]=0; } }
void shell_set_hostname(const char* h) { if (h) { strncpy(g_hostname,h,sizeof(g_hostname)-1); g_hostname[31]=0; } }
void shell_set_cwd(const char* p)      { if (p) { strncpy(g_cwd,p,sizeof(g_cwd)-1); g_cwd[127]=0; } }

static void shell_print_prompt(void) {
    fb_console_set_color(0x0000FF00, 0x00000000);
    printk("%s", g_username);
    printk("@%s", g_hostname);
    printk(":%s", g_cwd);

    fb_console_set_color(0x00FFFFFF, 0x00000000);
    printk("$ ");
    fb_console_flush();
}

void shell_init(void) {
    printk("KuvixOS Shell V2 Hazir!\n");
    printk("Komutlar icin 'help' yazabilirsiniz.\n");
    fb_console_flush();

    g_len = 0;
    g_line[0] = 0;
    g_prompt = 0;
}

void shell_tick(void) {
    /* şimdilik boş (cursor blink vs burada olur) */
}

/* NEW: session -> shell input */
void shell_handle_scancode(uint16_t ev) {
    uint8_t sc = (uint8_t)ev;

    /* break ignore */
    if (sc & 0x80) return;

    if (!g_prompt) {
        shell_print_prompt();
        g_prompt = 1;
    }

    /* char decode: senin kbd sistemi layout çözüyor */
    char c = kbd_get_char();
    if (!c) return;

    /* ENTER */
    if (c == '\n' || c == '\r') {
        g_line[g_len] = '\0';
        printk("\n");
        fb_console_flush();

        if (g_len > 0) {
            commands_execute(g_line);
            fb_console_flush();
        }

        g_len = 0;
        g_line[0] = 0;
        g_prompt = 0;
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

    /* normal */
    uint8_t uc = (uint8_t)c;
    if (uc >= 32 && g_len < (int)sizeof(g_line) - 1) {
        g_line[g_len++] = (char)uc;
        g_line[g_len] = '\0';
        printk("%c", (unsigned char)uc);
        fb_console_flush();
    }
}
