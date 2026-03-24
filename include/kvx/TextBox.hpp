#pragma once

extern "C" {
#include <kernel/exec/kef_minimal_runtime.h>
}

class TextBox {
public:
    TextBox(kef_minimal_state_t* st = nullptr, kef_widget_t* w = nullptr)
        : state(st), widget(w) {}

    bool isValid() const {
        return state != nullptr && widget != nullptr;
    }

    const char* getId() const {
        return widget ? widget->id : "";
    }

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

private:
    kef_minimal_state_t* state;
    kef_widget_t* widget;
};