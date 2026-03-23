#pragma once

#include "UIElement.hpp"

extern "C" {
#include <ui/controls/textbox2.h>
}

class TextBox : public UIElement {
public:
    explicit TextBox(ui_control_t* c = nullptr) : UIElement(c) {}

    const char* getText() const {
        if (!ctrl) return "";
        return textbox2_get_text((textbox2_t*)ctrl);
    }

    void setText(const char* text) {
        if (!ctrl) return;
        textbox2_set_text((textbox2_t*)ctrl, text);
    }
};