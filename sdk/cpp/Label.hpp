#pragma once

#include "UIElement.hpp"

extern "C" {
#include <ui/controls/label2.h>
}

class Label : public UIElement {
public:
    explicit Label(ui_control_t* c = nullptr) : UIElement(c) {}

    void setText(const char* text) {
        if (!ctrl) return;
        ui_label2_t* lbl = (ui_label2_t*)ctrl;
        ui_label2_set_text(lbl, text);
    }

    const char* getText() const {
        if (!ctrl) return "";
        ui_label2_t* lbl = (ui_label2_t*)ctrl;
        return lbl->text ? lbl->text : "";
    }
};