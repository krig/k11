#include "../print.hpp"
#include <iostream>
#include <string>

using namespace std;
using namespace print;

int main() {
    pr("Hello, world!\n");
    pn("Hello, world!");
    string world = "World";
    pr("Hello, %s!\n", world);
    pn("Hello, %s!", false);
    pn("Char: %c %c", 'a', 65);
    pn("Numbers: %#x %i %03d %u", 300, 50, -30, 18U);
    pn("Pointer: %08p", (char*)nullptr);

    auto s = to_str('!');
    auto s2 = to_str(s, "hello", 2, 'U', 10101);
    pn("%s", s2);
}
