#pragma once

#include <kvx/Button.hpp>
#include <kvx/Label.hpp>
#include <kvx/TextBox.hpp>
#include <kvx/ComboBox.hpp>

extern "C" {
#include <kernel/exec/kef_minimal_runtime.h>
}

class App {
private:
    kef_minimal_state_t* state;

public:
    explicit App(kef_minimal_state_t* st = nullptr) : state(st) {}

    void setState(kef_minimal_state_t* st) {
        state = st;
    }

    kef_minimal_state_t* getState() const {
        return state;
    }

    template<typename T>
    T getElementById(const char* id) {
        if (!state || !id) {
            return T(nullptr, nullptr);
        }

        kef_widget_t* w = kef_get_widget_ptr(state, id);
        return T(state, w);
    }
};