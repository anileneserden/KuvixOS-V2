#pragma once

#include "UIElement.hpp"

class Label : public UIElement {
public:
    Label(kef_minimal_state_t* st = nullptr, kef_widget_t* w = nullptr)
        : UIElement(st, w) {}

    void setText(const char* text) {
        if (!isValid()) return;
        kef_set_text(state, widget->id, text);
    }

    const char* getText() const {
        if (!isValid()) return "";
        return widget->text;
    }
};