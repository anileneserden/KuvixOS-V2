#include "commands.h"
#include <kernel/printk.h>

/* command prototypes */
void cmd_help(int argc, char** argv);
void cmd_service(int argc, char** argv);

typedef struct {
    const char*   name;
    command_fn_t  fn;
} command_entry_t;

static command_entry_t commands[] = {
    { "help",    cmd_help    },
    { "service", cmd_service },
};

static const int commands_count =
    sizeof(commands) / sizeof(commands[0]);

static int streq(const char* a, const char* b) {
    while (*a && *b && *a == *b) {
        a++; b++;
    }
    return (*a == 0 && *b == 0);
}

static int split_line(char* line, char** argv, int max) {
    int argc = 0;
    char* p = line;

    while (*p && argc < max) {
        while (*p == ' ') p++;
        if (!*p) break;

        argv[argc++] = p;

        while (*p && *p != ' ') p++;
        if (*p) *p++ = 0;
    }
    return argc;
}

void commands_execute(char* line) {
    char* argv[8];
    int argc = split_line(line, argv, 8);
    if (argc == 0) return;

    for (int i = 0; i < commands_count; i++) {
        if (streq(argv[0], commands[i].name)) {
            commands[i].fn(argc, argv);
            return;
        }
    }

    printk("Unknown command. Try 'help'. \n");
}

#include <kernel/printk.h>

void cmd_help(int argc, char** argv) {
    printk("KuvixOS V2 Yardim Menusu\n");
    printk("help - Bu yardim mesajini gosterir\n");
}

void cmd_service(int argc, char** argv) {
    // service.c'deki listeleme fonksiyonunu çağırabilirsin
    // extern void services_list(void);
    // services_list();
    printk("Servis yonetimi henuz tam hazir degil.\n");
}