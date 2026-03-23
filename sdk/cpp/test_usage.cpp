#include <App.hpp>

extern "C" void kef_cpp_smoke_test(kef_minimal_state_t* st) {
    App app(st);

    auto cppLabel = app.getElementById<Label>("cppLabel");

    if (!cppLabel.isValid()) {
        kef_set_text(st, "statusLabel", "CPP LABEL YOK");
        return;
    }

    kef_set_text(st, "cppLabel", "CPP LABEL OK");
}