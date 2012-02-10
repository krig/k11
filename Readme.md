# k11 - playing with C++11

Where I play with fancy new C++11 features, and also make a library of (hopefully) useful things.

## What's there now

* src/tests/* - various small tests of C++11 features.

* **print.hpp** - A typechecked printf/fprintf. Adapted from a lecture by Andrei Alexandrescu.

Examples of use:

    using namespace print;

    pr("Hello, world!\n"); // same as printf
    pn("Hello, world!"); // printf + \n
    string world = "World";
    pr("Hello, %s!\n", world); // print std::strings directly
    pn("Hello, %s!", false); // print bools into %s (prints true or false)

    // most regular format strings should pass
    pn("Char: %c %c", 'a', 65);
    pn("Numbers: %#x %i %03d %u", 300, 50, -30, 18U);
    pn("Pointer: %08p", (char*)nullptr);



