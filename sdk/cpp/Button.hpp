#pragma once

#include "UIElement.hpp"

extern "C" {
#include <ui/controls/button2.h>
}

class Button : public UIElement {
public:
    explicit Button(ui_control_t* c = nullptr) : UIElement(c) {}
};