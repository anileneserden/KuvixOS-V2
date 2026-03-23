#pragma once

#include "UIElement.hpp"

class TextBox : public UIElement {
public:
    TextBox(kef_minimal_state_t* st = nullptr, kef_widget_t* w = nullptr)
        : UIElement(st, w) {}

    const char* getText() const {
        if (!isValid()) return "";
        return widget->value;
    }

    void setText(const char* text) {
        if (!isValid()) return;

        if (!text) {
            widget->value[0] = 0;
            widget->value_len = 0;
            return;
        }

        int i = 0;
        for (; text[i] && i < (int)sizeof(widget->value) - 1; ++i) {
            widget->value[i] = text[i];
        }
        widget->value[i] = 0;
        widget->value_len = i;
    }
};