#include <kuvixos.hpp>

int main(void) {
    Console console;
    Args args;

    console.WriteLine("Hello World!");

    for (int i = 0; i < args.Count(); i++) {
        console.Write("arg: ");
        console.WriteLine(args.Get(i));
    }

    return 0;
}