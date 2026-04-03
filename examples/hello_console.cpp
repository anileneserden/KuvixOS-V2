#include <sdk/kef/prelude.hpp>

class HelloConsole : public Console {
public:
    void OnCreate() override {
        PrintLine("Console app created.");
    }

    int Main() override {
        Print("Hello World from Console app\n");
        PrintLine("This is the first sdk-core console example.");
        return 0;
    }
};

KEF_APP(HelloConsole)