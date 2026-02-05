#ifndef MESSAGEBOX_H
#define MESSAGEBOX_H

#include <stdint.h>
#include <stdbool.h>

// 1. Enum Tipleri (İsimler çakışmasın diye MB_ ön ekiyle)
typedef enum {
    MB_BTNS_OK,
    MB_BTNS_YESNO,
    MB_BTNS_RETRYCANCEL
} MB_BTNS_T;

typedef enum {
    MB_ICON_NONE,
    MB_ICON_INFO,
    MB_ICON_WARNING,
    MB_ICON_ERROR
} MB_ICON_T;

// 2. Namespace Yapısı (C# taklidi)
typedef struct {
    void (*Show)(const char* title, const char* text, MB_ICON_T icon, MB_BTNS_T buttons);
    void (*Close)(void);
} MessageBox_Namespace;

// 3. Sabitleri tutan yapı (MessageBoxButtons.OK yazabilmek için)
typedef struct {
    MB_BTNS_T OK;
    MB_BTNS_T YesNo;
} MessageBoxButtons_Wrapper;

// 4. Dışarıya açılan global nesneler
extern MessageBox_Namespace MessageBox;
extern MessageBoxButtons_Wrapper MessageBoxButtons;

// Sistem fonksiyonları (Çizim ve Fare için)
void messagebox_draw(void);
void messagebox_handle_mouse(int mx, int my, bool pressed);

#endif