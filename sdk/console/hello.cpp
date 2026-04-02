#include <kuvixos.hpp>

#include <File.hpp>

int main(void) {
    Console console;
    Args args;
    char buffer[512];
    uint32_t readLen = 0;

    console.WriteLine("Hello World!");

    for (int i = 0; i < args.Count(); i++) {
        console.Write("arg: ");
        console.WriteLine(args.Get(i));
    }

    if (args.Count() < 2) {
        console.WriteLine("usage: ./hello.kef <file.txt>");
        return 0;
    }

    if (!File::Read(args.Get(1), buffer, sizeof(buffer) - 1, &readLen)) {
        console.WriteLine("file read failed");
        return 1;
    }

    buffer[readLen] = 0;
    console.WriteLine("file content:");
    console.WriteLine(buffer);

    return 0;
}