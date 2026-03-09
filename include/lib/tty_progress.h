#pragma once

#include <stdint.h>

void tty_progress_begin(const char* title);
void tty_progress_update(uint32_t current, uint32_t total);
void tty_progress_step(const char* step, uint32_t current, uint32_t total);
void tty_progress_end(void);