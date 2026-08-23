// lib/shell/shell.c
#include <lib/shell.h>

#include <kernel/printk.h>
#include <kernel/kbd.h>
#include <lib/commands.h>
#include <kernel/drivers/video/fb_console.h>
#include <lib/string.h>
#include <stdint.h>

#include <kernel/fs/vfs.h>

#include <kernel/drivers/input/keyboard.h>

#include <init/session.h>

// ✅ Dışarıdan passwd modülünün durumunu ve tuş işleyicisini çağırabilmek için:
extern int passwd_is_active(void);
extern int passwd_handle_scancode(uint16_t scancode);

// ------------------------------------------------------------
// Identity + cwd
// ------------------------------------------------------------
static char g_username[32] = "root";
static char g_hostname[32] = "kuvix";
static char g_cwd[128]     = "/home";
static int g_dirty = 0;

// ------------------------------------------------------------
// Line editor state
// ------------------------------------------------------------
static char g_line[128];
static int  g_len = 0;

// ------------------------------------------------------------
// Commands routing
// ------------------------------------------------------------
static void shell_cmd_out(void* u, const char* s) {
    (void)u;
    if (!s) return;
    printk("%s", s);
    g_dirty = 1;
}

static void shell_cmd_clear(void* u) {
    (void)u;
    fb_console_clear(); 
    g_dirty = 1;
}

// ------------------------------------------------------------
// Prompt
// ------------------------------------------------------------
void shell_print_prompt(void) {
    user_session_t* session = session_get_current();
    uint32_t current_uid = session ? session->uid : 1; 

    fb_console_set_color(0x00FFFFFF, 0x00000000);

    const char* active_user = (session && session->username[0]) ? session->username : g_username;
    printk("%s", active_user);
    printk("@%s:", g_hostname);

    const char* cwd = vfs_get_cwd();
    const char* home_dir = (session && session->home_dir[0]) ? session->home_dir : "/home/anil";
    int home_len = strlen(home_dir);

    // Ev dizini kontrolü (Kullanıcı kim olursa olsun, kendi home dizinindeyse ~ yazdır)
    if (strcmp(cwd, home_dir) == 0 || (strncmp(cwd, home_dir, home_len) == 0 && cwd[home_len] == '\0')) {
        printk("~");
    } 
    else if (strncmp(cwd, home_dir, home_len) == 0 && cwd[home_len] == '/') {
        // Ev dizininin altındaki alt klasörler (~/klasor)
        if (cwd[home_len + 1] == '\0') {
            printk("~");
        } else {
            printk("~%s", cwd + home_len);
        }
    }
    else {
        // Ev dizini dışındaysa tam yolu yazdır (örn: /etc, /root vb.)
        printk("%s", cwd);
    }

    // Yetkiye göre prompt işareti (# veya $)
    if (current_uid == 0) {
        printk("# ");
    } else {
        printk("$ ");
    }
    
    g_dirty = 1;
}

// ------------------------------------------------------------
// Echo helpers
// ------------------------------------------------------------
static inline void echo_char(uint8_t c) {
    printk("%c", (unsigned char)c);
    g_dirty = 1;
}

static inline void echo_newline(void) {
    printk("\n");
    g_dirty = 1;
}

static inline void echo_backspace(void) {
    printk("\b \b");
    g_dirty = 1;
}

// ------------------------------------------------------------
// Public setters
// ------------------------------------------------------------
void shell_set_username(const char* u) {
    if (!u) return;
    strncpy(g_username, u, sizeof(g_username) - 1);
    g_username[sizeof(g_username) - 1] = 0;
}

void shell_set_hostname(const char* h) {
    if (!h) return;
    strncpy(g_hostname, h, sizeof(g_hostname) - 1);
    g_hostname[sizeof(g_hostname) - 1] = 0;
}

void shell_set_cwd(const char* p) {
    if (!p) return;
    strncpy(g_cwd, p, sizeof(g_cwd) - 1);
    g_cwd[sizeof(g_cwd) - 1] = 0;
}

// ------------------------------------------------------------
// Lifecycle
// ------------------------------------------------------------
void shell_init(void) {
    commands_set_output(shell_cmd_out, NULL);
    commands_set_clear(shell_cmd_clear, NULL);

    fb_console_clear();

    printk("KuvixOS Shell V2 Hazir!\n");
    printk("Komutlar icin 'help' yazabilirsiniz.\n\n");

    g_len = 0;
    g_line[0] = 0;
    
    shell_print_prompt();
    fb_console_flush();
    g_dirty = 0;
}

// ------------------------------------------------------------
// Tick
// ------------------------------------------------------------
void shell_tick(void) {
    if (g_dirty) {
        fb_console_flush();
        g_dirty = 0;
    }
}

// ------------------------------------------------------------
// Input
// ------------------------------------------------------------
void shell_handle_scancode(uint16_t ev) {
    // ✅ Eğer interaktif passwd modundaysak, tuşları doğrudan passwd komutuna yönlendir!
    if (passwd_is_active()) {
        passwd_handle_scancode(ev);
        return;
    }

    uint8_t sc = (uint8_t)(ev & 0xFF);
    uint8_t is_e0 = ((ev & 0xFF00) == 0xE000);

    if (is_e0) return;
    if (sc & 0x80) return;

    char c = kbd_scancode_to_ascii(sc);
    if (!c) return;

    if (c == '\n' || c == '\r') {
        g_line[g_len] = 0;
        echo_newline();

        if (g_len > 0) {
            commands_execute(g_line);
        }

        g_len = 0;
        g_line[0] = 0;

        // ✅ EĞER passwd komutu aktif hale geldiyse, hemen prompt BASMA!
        if (passwd_is_active()) {
            return;
        }

        shell_print_prompt();
        return;
    }

    if (c == '\b' || (uint8_t)c == 8 || (uint8_t)c == 127) {
        if (g_len > 0) {
            g_len--;
            g_line[g_len] = 0;
            echo_backspace();
        }
        return;
    }

    uint8_t uc = (uint8_t)c;
    if (uc >= 32) {
        if (g_len < (int)sizeof(g_line) - 1) {
            g_line[g_len++] = (char)uc;
            g_line[g_len] = 0;
            echo_char(uc);
        }
    }
}