#pragma once

extern "C" {
#include <kernel/exec/kef_minimal_runtime.h>
}

class Label {
public:
    Label(kef_minimal_state_t* st = nullptr, kef_widget_t* w = nullptr)
        : state(st), widget(w) {}

    bool isValid() const {
        return state != nullptr && widget != nullptr;
    }

    const char* getId() const {
        return widget ? widget->id : "";
    }

    void setText(const char* text) {
        if (!isValid() || !text) return;
        kef_set_text(state, widget->id, text);
    }

    const char* getText() const {
        if (!isValid()) return "";
        return widget->text;
    }

private:
    kef_minimal_state_t* state;
    kef_widget_t* widget;
};