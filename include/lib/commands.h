#ifndef COMMANDS_H
#define COMMANDS_H

typedef void (*command_fn_t)(int argc, char** argv);

typedef struct {
    const char* name;
    command_fn_t fn;
    const char* help;
} command_t;

// Bu makro, komutu otomatik olarak linker bölümüne kaydeder
#define REGISTER_COMMAND(name, func, help_text) \
    command_t _cmd_##name __attribute__((section(".cmd_section"))) = {#name, func, help_text}

void commands_execute(char* line);

#endif