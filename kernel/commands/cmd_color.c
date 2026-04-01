#include <lib/commands.h>
#include <lib/string.h>
#include <kernel/drivers/video/fb_console.h>

// Global renk durumu
static unsigned int g_fg_color = 0x00FFFFFF; // beyaz
static unsigned int g_bg_color = 0x00000000; // siyah

// Basit hex parser
static unsigned int parse_hex(const char* s) {
    unsigned int val = 0;
    if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) s += 2;
    while (*s) {
        char c = *s++;
        val <<= 4;
        if (c >= '0' && c <= '9') val |= (c - '0');
        else if (c >= 'a' && c <= 'f') val |= (c - 'a' + 10);
        else if (c >= 'A' && c <= 'F') val |= (c - 'A' + 10);
        else break;
    }
    return val;
}

// color komutu
void cmd_color(int argc, char** argv) {
    if (argc == 2 && strcmp(argv[1], "d") == 0) {
        g_fg_color = 0x00FFFFFF;
        g_bg_color = 0x00000000;
        fb_console_set_color(g_fg_color, g_bg_color);
        commands_puts("Varsayilan renge donuldu.\n");
        return;
    }

    if (argc < 2) {
        commands_puts("Kullanim: color <hex_fg> [hex_bg] veya color d\n");
        return;
    }

    g_fg_color = parse_hex(argv[1]);
    g_bg_color = (argc > 2) ? parse_hex(argv[2]) : 0x00000000;
    fb_console_set_color(g_fg_color, g_bg_color);
    commands_puts("Renk degisti.\n");
}

// Global renkleri diğer dosyalardan kullanabilmek için getter
unsigned int color_get_fg(void) { return g_fg_color; }
unsigned int color_get_bg(void) { return g_bg_color; }

REGISTER_COMMAND(color, cmd_color, "Konsol rengini degistirir");
