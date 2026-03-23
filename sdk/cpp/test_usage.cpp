#include <App.hpp>
#include <lib/string.h>

extern "C" void kef_cpp_smoke_test(kef_minimal_state_t* st) {
    App app(st);

    auto combo = app.getElementById<ComboBox>("countryCombo");

    if (!combo.isValid()) {
        kef_set_text(st, "statusLabel", "COMBO YOK");
        return;
    }

    const char* txt = combo.getSelectedText();
    if (txt && txt[0]) {
        kef_set_text(st, "cppLabel", txt);
    } else {
        kef_set_text(st, "cppLabel", "BOS");
    }
}

extern "C" void kef_cpp_on_click(kef_minimal_state_t* st, const char* id) {
    if (!st || !id) return;

    App app(st);

    if (strcmp(id, "okButton") == 0) {
        auto cppLabel = app.getElementById<Label>("cppLabel");

        if (!cppLabel.isValid()) {
            kef_set_text(st, "statusLabel", "CPP LABEL YOK");
            return;
        }

        cppLabel.setText("butonOnClick calisti");
    }
}