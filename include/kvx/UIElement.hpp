#pragma once

extern "C" {
#include <kernel/exec/kef_minimal_runtime.h>
}

class UIElement {
protected:
    kef_minimal_state_t* state;
    kef_widget_t* widget;

public:
    UIElement(kef_minimal_state_t* st = nullptr, kef_widget_t* w = nullptr)
        : state(st), widget(w) {}

    bool isValid() const {
        return state != nullptr && widget != nullptr;
    }

    const char* getId() const {
        return widget ? widget->id : "";
    }

    kef_widget_t* raw() const {
        return widget;
    }
};