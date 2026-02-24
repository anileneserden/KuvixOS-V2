#include <lib/commands.h>
#include <lib/shell.h>

void cmd_heredoc(int argc, char** argv) {
    if (argc < 3) {
        commands_puts("Kullanim: heredoc <dosya> <EOF>\n");
        return;
    }

    // argv[1] = path, argv[2] = token
    shell_begin_heredoc(argv[1], argv[2]);
}

REGISTER_COMMAND(heredoc, cmd_heredoc, "Cok satirli dosya yaz (EOF ile bitir)");