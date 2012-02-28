// constructing a new sexpr type based on unions
// instead of boost::variant

#include <string>
#include <iostream>
#include "intrusive_ptr.hpp"
#include "value.hpp"
#include "symbol.hpp"
#include "cell.hpp"
#include "cellgc.hpp"

namespace {
    cellgc cellGC;
}

struct value_impl : public value {
    virtual ~value_impl() {}
    virtual const char* name() const { return _name; }
    virtual void incref() { ++_rc; }
    virtual void decref() { if (--_rc == 0) delete this; }

protected:
    value_impl(const char* name_)
        : _name(name_), _rc(0) {}

private:
    const char* _name;
    int _rc;
};

template <typename Fn, int Nslots>
struct proc_value : public value_impl {
    proc_value() : value_impl("proc") { cellGC.addroot(&expr); /* add env cell references as roots */ }
    ~proc_value() { cellGC.rmroot(&expr); }
    value* _parent;
    cell* expr;
    cell::slot env[Nslots];
    uint16_t envt[Nslots];
};

struct file_value : public value_impl {
    file_value() : value_impl("file"), _file(0) {
        std::cerr << "creating file value" << std::endl;
    }
    virtual ~file_value() {
        std::cerr << "destroying file value" << std::endl;
        if (_file) fclose(_file);
    }
    FILE* _file;
};

int main() {

    cell* a = cellGC.alloc(1);
    cell* b = cellGC.alloc(2);
    cell* c = cellGC.alloc(3);

    cellGC.addroot(&a);
    cellGC.addroot(&b);
    cellGC.addroot(&c);

    a->car(symbol("hello"));
    a->cdr(b);
    b->car(1);
    b->cdr(b+1);
    (b+1)->car(value_ptr(new file_value));
    (b+1)->cdr(c);
    c->car(3.2);
    c->cdr(c+1);
    (c+1)->car("world");
    (c+1)->nil_cdr();
    (c+2)->car(value_ptr(new file_value));
    (c+2)->nil_cdr();

    std::cout << a << "\n";
    std::cout << (c+2) << "\n";

    for (int i = 0; i < 2000; ++i) {
        cell* lst = cellGC.alloc_list(20);
        cell* k = lst;
        for (; !k->cdr_is_nil(); ++k)
            k->car(rand()%(i+1));
        k->car(i);
    }

    std::cout << a << "\n";

    cellGC.rmroot(&a);
    cellGC.rmroot(&b);
    cellGC.rmroot(&c);
}
