#pragma once

#include "UIElement.hpp"

class Button : public UIElement {
public:
    Button(kef_minimal_state_t* st = nullptr, kef_widget_t* w = nullptr)
        : UIElement(st, w) {}
};