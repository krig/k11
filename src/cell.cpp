#include "cell.hpp"
#include "value.hpp"

cell& cell::operator=(const cell& c) {
    // todo: exception handling
    if (&c == this)
        return *this;
    if (typa == c.typa) {
        switch (c.typa) {
        case Nil: break;
        case Int: a.i = c.a.i; break;
        case Float: a.f = c.a.f; break;
        case Double: a.d = c.a.d; break;
        case Cell: a.c = c.a.c; break;
        case Value: a.v = c.a.v; break;
        case String: a.s = c.a.s; break;
        case Symbol: a.y = c.a.y; break;
        }
    }
    else {
        destroy();
        switch (typa = c.typa) {
        case Nil: break;
        case Int: a.i = c.a.i; break;
        case Float: a.f = c.a.f; break;
        case Double: a.d = c.a.d; break;
        case Cell: a.c = c.a.c; break;
        case Value: new (&a.v) value_ptr(c.a.v); break;
        case String: new (&a.s) std::string(c.a.s); break;
        case Symbol: a.y = c.a.y; break;
        }
    }
    if (typb == c.typb) {
        switch (c.typb) {
        case Nil: break;
        case Int: b.i = c.b.i; break;
        case Float: b.f = c.b.f; break;
        case Double: b.d = c.b.d; break;
        case Cell: b.c = c.b.c; break;
        case Value: b.v = c.b.v; break;
        case String: b.s = c.b.s; break;
        case Symbol: b.y = c.b.y; break;
        }
    }
    else {
        switch (typb = c.typb) {
        case Nil: break;
        case Int: b.i = c.b.i; break;
        case Float: b.f = c.b.f; break;
        case Double: b.d = c.b.d; break;
        case Cell: b.c = c.b.c; break;
        case Value: new (&b.v) value_ptr(c.b.v); break;
        case String: new (&b.s) std::string(c.b.s); break;
        case Symbol: b.y = c.b.y; break;
        }
    }
    flags = c.flags;
    return *this;
}

cell& cell::operator=(cell&& c) {
    if (this != &c) {
        destroy();

        switch (c.typa) {
        case Nil: break;
        case Int: a.i = c.a.i; break;
        case Float: a.f = c.a.f; break;
        case Double: a.d = c.a.d; break;
        case Cell: a.c = c.a.c; break;
        case Value: new (&a.v) value_ptr(std::move(c.a.v)); break;
        case String: new (&a.s) std::string(std::move(c.a.s)); break;
        case Symbol: a.y = c.a.y; break;
        }
        switch (c.typb) {
        case Nil: break;
        case Int: b.i = c.b.i; break;
        case Float: b.f = c.b.f; break;
        case Double: b.d = c.b.d; break;
        case Cell: b.c = c.b.c; break;
        case Value: new (&b.v) value_ptr(std::move(c.b.v)); break;
        case String: new (&b.s) std::string(std::move(c.b.s)); break;
        case Symbol: b.y = c.b.y; break;
        }
        flags = c.flags;
        typa = c.typa;
        typb = c.typb;
        // clear the other cell
        c.flags = 0;
        c.typa = Nil;
        c.typb = Nil;
    }
    return *this;
}

void cell::destroy() {
    using std::string;
    using util::intrusive_ptr;
    switch(typa) {
    default: break;
    case Value: a.v.~intrusive_ptr(); break;
    case String: a.s.~string(); break;
    }
    switch(typb) {
    default: break;
    case Value: b.v.~intrusive_ptr(); break;
    case String: b.s.~string(); break;
    }
}
void cell::destroycar() {
    using std::string;
    using util::intrusive_ptr;
    switch(typa) {
    default: break;
    case Value: a.v.~intrusive_ptr(); break;
    case String: a.s.~string(); break;
    }
}
void cell::destroycdr() {
    using std::string;
    using util::intrusive_ptr;
    switch(typb) {
    default: break;
    case Value: b.v.~intrusive_ptr(); break;
    case String: b.s.~string(); break;
    }
}

bool cell::operator==(const cell& c) const {
    if (typa == c.typa && typb == c.typb && flags == c.flags) {
        switch (typa) {
        case Int: if (a.i != c.a.i) return false; break;
        case Float: if (a.f != c.a.f) return false; break;
        case Double: if (a.d != c.a.d) return false; break;
        case Cell: if (a.c != c.a.c) return false; break;
        case Value: if (a.v != c.a.v) return false; break;
        case String: if (a.s != c.a.s) return false; break;
        case Symbol: if (a.y != c.a.y) return false; break;
        default: break;
        };
        switch (typa) {
        case Int: if (b.i != c.b.i) return false; break;
        case Float: if (b.f != c.b.f) return false; break;
        case Double: if (b.d != c.b.d) return false; break;
        case Cell: if (b.c != c.b.c) return false; break;
        case Value: if (b.v != c.b.v) return false; break;
        case String: if (b.s != c.b.s) return false; break;
        case Symbol: if (b.y != c.b.y) return false; break;
        default: break;
        };
        return true;
    }
    return false;
}


void cell::car(int i) {
    if (typa != Int) {
        destroycar();
        typa = Int;
    }
    a.i = i;
}

void cell::car(float f) {
    if (typa != Float) {
        destroycar();
        typa = Float;
    }
    a.f = f;
}

void cell::car(double d) {
    if (typa != Double) {
        destroycar();
        typa = Double;
    }
    a.d = d;
}

void cell::car(std::string&& s) {
    if (typa != String) {
        destroycar();
        typa = String;
        new (&a.s) std::string(std::move(s));
    }
    else {
        a.s = std::move(s);
    }
}

void cell::car(value_ptr&& v) {
    if (typa != Value) {
        destroycar();
        typa = Value;
        new (&a.v) value_ptr(std::move(v));
    }
    else {
        b.v = std::move(v);
    }
}


void cell::car(cell* c) {
    if (typa != Cell) {
        destroycar();
        typa = Cell;
    }
    a.c = c;
}

void cell::car(const symbol& sym) {
    if (typa != Symbol) {
        destroycar();
        typa = Symbol;
    }
    a.y = sym;
}

void cell::nil_car() {
    if (typa != Nil) {
        destroycar();
        typa = Nil;
    }
}

void cell::cdr(int i) {
    if (typb != Int) {
        destroycdr();
        typb = Int;
    }
    b.i = i;
}

void cell::cdr(float f) {
    if (typb != Float) {
        destroycdr();
        typb = Float;
    }
    b.f = f;
}

void cell::cdr(double d) {
    if (typb != Double) {
        destroycdr();
        typb = Double;
    }
    b.d = d;
}

void cell::cdr(std::string&& s) {
    if (typb != String) {
        destroycdr();
        typb = String;
        new (&b.s) std::string(std::move(s));
    }
    else {
        b.s = std::move(s);
    }
}

void cell::cdr(value_ptr&& v) {
    if (typb != Value) {
        destroycdr();
        typb = Value;
        new (&b.v) value_ptr(std::move(v));
    }
    else {
        b.v = std::move(v);
    }
}

void cell::cdr(cell* c) {
    if (typb != Cell) {
        destroycdr();
        typb = Cell;
    }
    b.c = c;
}

void cell::cdr(const symbol& sym) {
    if (typb != Symbol) {
        destroycdr();
        typb = Symbol;
    }
    b.y = sym;
}

void cell::nil_cdr() {
    if (typb != Nil) {
        destroycdr();
        typb = Nil;
    }
}

std::ostream& operator<<(std::ostream& s, cell* c) {
    s << "(";
    while (true) {
        switch (c->typa) {
        case cell::Nil: s << "nil"; break;
        case cell::Int: s << *car<int>(c); break;
        case cell::Float: s << *car<float>(c); break;
        case cell::Double: s << *car<double>(c); break;
        case cell::String: s << car<std::string>(c)->c_str(); break;
        case cell::Symbol: s << car<symbol>(c)->c_str(); break;
        case cell::Value: s << "<value>"; break;
        case cell::Cell: s << c; break;
        }

        switch (c->typb) {
        case cell::Nil: s << ")"; break;
        case cell::Int: s << " . " << *cdr<int>(c) << ")"; break;
        case cell::Float: s << " . " << *cdr<float>(c) << ")"; break;
        case cell::Double: s << " . " << *cdr<double>(c) << ")"; break;
        case cell::String: s << " . " << cdr<std::string>(c)->c_str() << ")"; break;
        case cell::Symbol: s << " . " << cdr<symbol>(c)->c_str() << ")"; break;
        case cell::Value: s << " . <value>)"; break;
        case cell::Cell: s << " "; break;
        }

        if (cdr<cell>(c)) {
            c = cdr<cell>(c);
            continue;
        }
        break;
    }

    return s;
}
