#pragma once

#include "Button.hpp"
#include "Label.hpp"
#include "TextBox.hpp"

extern "C" {
#include <ui/controls/control.h>
}

class App {
private:
    ui_control_t* root;

public:
    explicit App(ui_control_t* rootControl = nullptr) : root(rootControl) {}

    void setRoot(ui_control_t* rootControl) {
        root = rootControl;
    }

    ui_control_t* getRoot() const {
        return root;
    }

    template<typename T>
    T getElementById(const char* id) {
        ui_control_t* found = ui_find_control_by_name(root, id);
        return T(found);
    }
};