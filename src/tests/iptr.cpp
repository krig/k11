#include "intrusive_ptr.hpp"
#include <iostream>

struct foo {
    foo() : x(0) {}
    ~foo() {
        std::cout << "deleted!" << std::endl;
    }
    int x;
};

void incref(foo* f) {
    ++f->x;
}

void decref(foo* f) {
    if (--f->x == 0)
        delete f;
}

int main() {
    util::intrusive_ptr<foo> f(new foo);
    if (f) {
        std::cout << f->x << std::endl;
    }
}
