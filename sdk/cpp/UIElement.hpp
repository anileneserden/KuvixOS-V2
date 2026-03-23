#pragma once

extern "C" {
#include <ui/controls/control.h>
}

class UIElement {
protected:
    ui_control_t* ctrl;

public:
    explicit UIElement(ui_control_t* c = nullptr) : ctrl(c) {}

    bool isValid() const {
        return ctrl != nullptr;
    }

    const char* getName() const {
        return ctrl ? ctrl->name : "";
    }

    ui_control_t* raw() const {
        return ctrl;
    }
};