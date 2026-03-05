// include/ui/tui_action.h
#ifndef UI_TUI_ACTION_H
#define UI_TUI_ACTION_H

#ifdef __cplusplus
extern "C" {
#endif

// Action string örnekleri:
//   "session:tty1"
//   "session:desktop"
//   "sys:reboot"
//   "sys:poweroff"
//   "app:terminal"   (ileride)
//
// Bu fonksiyon action string'ini parse eder ve ilgili işi yapar.
void tui_execute_action(const char* action);

#ifdef __cplusplus
}
#endif

#endif // UI_TUI_ACTION_H