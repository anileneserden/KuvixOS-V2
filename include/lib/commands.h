#ifndef COMMANDS_H
#define COMMANDS_H

#include <stdint.h>

// Komut fonksiyon tipi
typedef void (*command_fn_t)(int argc, char** argv);

typedef struct {
    const char* name;
    command_fn_t fn;
    const char* help;
} command_t;

// 'used' özniteliği linker'ın bu veriyi silmesini engeller
#define REGISTER_COMMAND(name, func, help_text) \
    command_t _cmd_##name __attribute__((section(".cmd_section"), used)) = {#name, func, help_text}

// Output yönlendirme (Shell/Terminal)
typedef void (*commands_out_fn_t)(void* user, const char* s);

// Clear yönlendirme (Shell/Terminal)
typedef void (*commands_clear_fn_t)(void* user);

void commands_set_output(commands_out_fn_t fn, void* user);
void commands_set_clear(commands_clear_fn_t fn, void* user);

const char* commands_get_cwd(void);
void commands_set_cwd(const char* path);

void commands_puts(const char* s);
void commands_putc(char c);
void commands_printf(const char* fmt, ...);

void commands_clear(void);

// Komut çalıştırıcı
void commands_execute(char* line);

#endif