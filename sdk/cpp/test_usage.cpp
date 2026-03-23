#include <ui/controls/control.h>
#include "App.hpp"

extern "C" void kef_cpp_smoke_test(ui_control_t* root) {
    App app(root);
    auto lbl = app.getElementById<Label>("statusLabel");
    if (lbl.isValid()) {
        lbl.setText("CPP OK");
    }
}