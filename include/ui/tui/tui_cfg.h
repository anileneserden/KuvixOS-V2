#ifndef UI_TUI_CFG_H
#define UI_TUI_CFG_H

// cfg dosyasını VFS üzerinden okuyup parse eder
// return: 1 success, 0 fail
int tui_load_cfg(const char* path);

// RAM’deki metni parse eder (debug/test için)
// return: 1 success, 0 fail
int tui_load_cfg_text(const char* text);

#endif