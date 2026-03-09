#ifndef MESSAGEBOX_H
#define MESSAGEBOX_H

#include <stdint.h>
#include <stdbool.h>

// ------------------------------------------------------------
// Button types
// ------------------------------------------------------------
typedef enum {
    MB_BTNS_OK,
    MB_BTNS_YESNO,
    MB_BTNS_RETRYCANCEL
} MB_BTNS_T;

// ------------------------------------------------------------
// Icon types
// ------------------------------------------------------------
typedef enum {
    MB_ICON_NONE,
    MB_ICON_INFO,
    MB_ICON_WARNING,
    MB_ICON_ERROR
} MB_ICON_T;

// ------------------------------------------------------------
// Result values
// ------------------------------------------------------------
typedef enum {
    MB_RES_NONE = 0,
    MB_RES_OK,
    MB_RES_YES,
    MB_RES_NO,
    MB_RES_RETRY,
    MB_RES_CANCEL
} MB_RES_T;

// ------------------------------------------------------------
// Namespace (C# style)
// ------------------------------------------------------------
typedef struct {
    void (*Show)(const char* title, const char* text, MB_ICON_T icon, MB_BTNS_T buttons);
    void (*Close)(void);
} MessageBox_Namespace;

// ------------------------------------------------------------
// Helper constants (MessageBoxButtons.OK)
// ------------------------------------------------------------
typedef struct {
    MB_BTNS_T OK;
    MB_BTNS_T YesNo;
    MB_BTNS_T RetryCancel;
} MessageBoxButtons_Wrapper;

// ------------------------------------------------------------
// Globals
// ------------------------------------------------------------
extern MessageBox_Namespace MessageBox;
extern MessageBoxButtons_Wrapper MessageBoxButtons;

// ------------------------------------------------------------
// System functions
// ------------------------------------------------------------
void messagebox_draw(void);
void messagebox_handle_mouse(int mx, int my, bool pressed);

bool messagebox_is_visible(void);

MB_RES_T messagebox_get_result(void);
void messagebox_reset_result(void);

#endif