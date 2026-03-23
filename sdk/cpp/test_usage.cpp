#include <App.hpp>
#include <lib/string.h>

extern "C" void kef_cpp_smoke_test(kef_minimal_state_t* st) {
    kef_set_text(st, "cppLabel", "CPP SMOKE OK");
}

extern "C" void kef_cpp_on_click(kef_minimal_state_t* st, const char* id) {
    if (!st || !id) return;

    if (strcmp(id, "okButton") == 0) {
        kef_set_text(st, "cppLabel", "Deneme");
    }
}