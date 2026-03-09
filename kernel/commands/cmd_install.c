#include <kernel/printk.h>
#include <lib/tty_progress.h>
#include <lib/commands.h>
#include <stdint.h>

static void fake_delay(void) {
    for (volatile uint32_t i = 0; i < 30000000u; i++) {
        __asm__ __volatile__("" ::: "memory");
    }
}

static void install_run_demo(void) {
    printk("KuvixOS install demo started.\n");

    tty_progress_begin("Installer Demo");

    for (uint32_t i = 0; i <= 100; i++) {
        tty_progress_step("Preparing disk", i, 100);
        fake_delay();
    }

    for (uint32_t i = 0; i <= 100; i++) {
        tty_progress_step("Writing system image", i, 100);
        fake_delay();
    }

    for (uint32_t i = 0; i <= 100; i++) {
        tty_progress_step("Creating user config", i, 100);
        fake_delay();
    }

    for (uint32_t i = 0; i <= 100; i++) {
        tty_progress_step("Finalizing install", i, 100);
        fake_delay();
    }

    tty_progress_end();

    printk("Installation completed.\n");
}

static void cmd_install(int argc, char** argv) {
    (void)argc;
    (void)argv;

    install_run_demo();
}

REGISTER_COMMAND(install, cmd_install, "Sistemi diske kurar (DEMO)");