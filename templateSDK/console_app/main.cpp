#include <sdk/kef/prelude.hpp>

class TemplateConsoleApp : public Console {
public:
    void OnCreate() override {
        PrintLine("Console app created.");
    }

    int Main() override {
        PrintLine("Hello from TemplateConsoleApp.");
        PrintLine("Replace this class name and application logic with your own code.");
        return 0;
    }
};

KEF_APP(TemplateConsoleApp)