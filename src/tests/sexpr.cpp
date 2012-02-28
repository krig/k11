// constructing a new sexpr type based on unions
// instead of boost::variant

#include <vector>
#include <string>
#include <unordered_set>
#include <unordered_map>
#include <algorithm>
#include <cassert>
#include <stdexcept>
#include <iostream>
#include "intrusive_ptr.hpp"

struct value {
    virtual ~value() {}
    virtual const char* name() const = 0;
    virtual void incref() = 0;
    virtual void decref() = 0;
};

inline void incref(value* v) { v->incref(); }
inline void decref(value* v) { v->decref(); }

typedef util::intrusive_ptr<value> value_ptr;

struct nil_cell {};

struct symbol {
    typedef std::unordered_set<std::string> table_type;
    static table_type table;

    explicit symbol(const char* s) {
        i = table.emplace(s).first;
    }

    const char* c_str() const {
        return i->c_str();
    }

    bool operator==(const symbol& s) const {
        return i == s.i;
    }

    bool operator!=(const symbol& s) const {
        return i != s.i;
    }

private:
    table_type::iterator i;
};

symbol::table_type symbol::table;

struct cell {
    // mark = in gc, survivor1 = survived 1 younggen collection
    // survivor2 = survived 2 younggen collections, old = in oldgen
    // symbols are interned strings. Interning is handled elsewhere
    enum Flag { GcMark = 1, GcSurvivor1 = 2, GcSurvivor2 = 4, GcOld = 8 };
    enum Typ { Nil, Int, Float, Double, Cell, Value, String, Symbol };
    union slot {
        slot() : i(0) {}
        ~slot() {}
        nil_cell n;
        int i;
        float f;
        double d;
        cell* c;
        value_ptr v;
        std::string s;
        symbol y;
    };

    cell() : flags(0), typa(Nil), typb(Nil) {
    }

    ~cell() {
        destroy();
    }

    cell(const cell& c) {
        *this = c;
    }

    cell(cell&& c) : flags(0), typa(Nil), typb(Nil) {
        *this = std::move(c);
    }

    cell& operator=(const cell& c) {
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

    cell& operator=(cell&& c) {
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

    void destroy() {
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
    void destroycar() {
        using std::string;
        using util::intrusive_ptr;
        switch(typa) {
        default: break;
        case Value: a.v.~intrusive_ptr(); break;
        case String: a.s.~string(); break;
        }
    }
    void destroycdr() {
        using std::string;
        using util::intrusive_ptr;
        switch(typb) {
        default: break;
        case Value: b.v.~intrusive_ptr(); break;
        case String: b.s.~string(); break;
        }
    }

    void car(int i) {
        if (typa != Int) {
            destroycar();
            typa = Int;
        }
        a.i = i;
    }

    void car(float f) {
        if (typa != Float) {
            destroycar();
            typa = Float;
        }
        a.f = f;
    }

    void car(double d) {
        if (typa != Double) {
            destroycar();
            typa = Double;
        }
        a.d = d;
    }

    void car(std::string&& s) {
        if (typa != String) {
            destroycar();
            typa = String;
            new (&a.s) std::string(std::move(s));
        }
        else {
            a.s = std::move(s);
        }
    }

    void car(value_ptr&& v) {
        if (typa != Value) {
            destroycar();
            typa = Value;
            new (&a.v) value_ptr(std::move(v));
        }
        else {
            b.v = std::move(v);
        }
    }


    void car(cell* c) {
        if (typa != Cell) {
            destroycar();
            typa = Cell;
        }
        a.c = c;
    }

    void car(const symbol& sym) {
        if (typa != Symbol) {
            destroycar();
            typa = Symbol;
        }
        a.y = sym;
    }

    void nil_car() {
        if (typa != Nil) {
            destroycar();
            typa = Nil;
        }
    }

    void cdr(int i) {
        if (typb != Int) {
            destroycdr();
            typb = Int;
        }
        b.i = i;
    }

    void cdr(float f) {
        if (typb != Float) {
            destroycdr();
            typb = Float;
        }
        b.f = f;
    }

    void cdr(double d) {
        if (typb != Double) {
            destroycdr();
            typb = Double;
        }
        b.d = d;
    }

    void cdr(std::string&& s) {
        if (typb != String) {
            destroycdr();
            typb = String;
            new (&b.s) std::string(std::move(s));
        }
        else {
            b.s = std::move(s);
        }
    }

    void cdr(value_ptr&& v) {
        if (typb != Value) {
            destroycdr();
            typb = Value;
            new (&b.v) value_ptr(std::move(v));
        }
        else {
            b.v = std::move(v);
        }
    }

    void cdr(cell* c) {
        if (typb != Cell) {
            destroycdr();
            typb = Cell;
        }
        b.c = c;
    }

    void cdr(const symbol& sym) {
        if (typb != Symbol) {
            destroycdr();
            typb = Symbol;
        }
        b.y = sym;
    }

    void nil_cdr() {
        if (typb != Nil) {
            destroycdr();
            typb = Nil;
        }
    }

    bool car_is_nil() const {
        return typa == Nil;
    }

    bool cdr_is_nil() const {
        return typb == Nil;
    }

    slot a;
    slot b;
    uint16_t flags;
    uint8_t typa;
    uint8_t typb;
};

template <typename T> const T* car(const cell* c);
template <typename T> const T* cdr(const cell* c);
template <typename T> T* car(cell* c);
template <typename T> T* cdr(cell* c);

template <> const int* car<int>(const cell* c) { return c->typa==cell::Int?&c->a.i:nullptr; }
template <> const float* car<float>(const cell* c) { return c->typa==cell::Float?&c->a.f:nullptr; }
template <> const double* car<double>(const cell* c) { return c->typa==cell::Double?&c->a.d:nullptr; }
template <> const std::string* car<std::string>(const cell* c) { return c->typa==cell::String?&c->a.s:nullptr; }
template <> const value_ptr* car<value_ptr>(const cell* c) { return c->typa==cell::Value?&c->a.v:nullptr; }
template <> const symbol* car<symbol>(const cell* c) { return c->typa==cell::Symbol?&c->a.y:nullptr; }
template <> const cell* car<cell>(const cell* c) { return c->typa==cell::Cell?c->a.c:nullptr; }

template <> const int* cdr<int>(const cell* c) { return c->typb==cell::Int?&c->b.i:nullptr; }
template <> const float* cdr<float>(const cell* c) { return c->typb==cell::Float?&c->b.f:nullptr; }
template <> const double* cdr<double>(const cell* c) { return c->typb==cell::Double?&c->b.d:nullptr; }
template <> const std::string* cdr<std::string>(const cell* c) { return c->typb==cell::String?&c->b.s:nullptr; }
template <> const value_ptr* cdr<value_ptr>(const cell* c) { return c->typb==cell::Value?&c->b.v:nullptr; }
template <> const symbol* cdr<symbol>(const cell* c) { return c->typb==cell::Symbol?&c->b.y:nullptr; }
template <> const cell* cdr<cell>(const cell* c) { return c->typb==cell::Cell?c->b.c:nullptr; }

template <> int* car<int>(cell* c) { return c->typa==cell::Int?&c->a.i:nullptr; }
template <> float* car<float>(cell* c) { return c->typa==cell::Float?&c->a.f:nullptr; }
template <> double* car<double>(cell* c) { return c->typa==cell::Double?&c->a.d:nullptr; }
template <> std::string* car<std::string>(cell* c) { return c->typa==cell::String?&c->a.s:nullptr; }
template <> value_ptr* car<value_ptr>(cell* c) { return c->typa==cell::Value?&c->a.v:nullptr; }
template <> symbol* car<symbol>(cell* c) { return c->typa==cell::Symbol?&c->a.y:nullptr; }
template <> cell* car<cell>(cell* c) { return c->typa==cell::Cell?c->a.c:nullptr; }

template <> int* cdr<int>(cell* c) { return c->typb==cell::Int?&c->b.i:nullptr; }
template <> float* cdr<float>(cell* c) { return c->typb==cell::Float?&c->b.f:nullptr; }
template <> double* cdr<double>(cell* c) { return c->typb==cell::Double?&c->b.d:nullptr; }
template <> std::string* cdr<std::string>(cell* c) { return c->typb==cell::String?&c->b.s:nullptr; }
template <> value_ptr* cdr<value_ptr>(cell* c) { return c->typb==cell::Value?&c->b.v:nullptr; }
template <> symbol* cdr<symbol>(cell* c) { return c->typb==cell::Symbol?&c->b.y:nullptr; }
template <> cell* cdr<cell>(cell* c) { return c->typb==cell::Cell?c->b.c:nullptr; }

template <int Len>
struct slotray {
    cell::slot slots[Len];
};

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

struct gcimpl;

struct cellgc {
    cellgc();
    ~cellgc();
    void collect(); // mark & sweep
    void addroot(cell** c);
    void rmroot(cell** c);
    cell* alloc(size_t len = 1);

    gcimpl* _impl;
} cellGC;

struct gcimpl {
    gcimpl();
    ~gcimpl();
    void collect(); // younggen collection
    void addroot(cell** c);
    void rmroot(cell** c);
    cell* alloc(size_t len);

private:

    void oldgen_collect();
    void expand_heap();
    void mark(cell* c);

    // this would be needed if multithreaded
    void stop_the_world();
    void start_the_world();

    std::unordered_set<cell**> _roots;
    std::vector<cell> _young[2];
    std::vector<cell> _old;
    std::unordered_map<cell*, cell*> _ptrs; // used while collecting
    int _current;
};

cellgc::cellgc() : _impl(new gcimpl) {
}
cellgc::~cellgc() {
    delete _impl;
}
void cellgc::collect() { _impl->collect(); } // mark & sweep
void cellgc::addroot(cell** c) { _impl->addroot(c); }
void cellgc::rmroot(cell** c) { _impl->rmroot(c); }
cell* cellgc::alloc(size_t len) { return _impl->alloc(len); }

gcimpl::gcimpl() : _roots() {
    const int YoungSize = 10*1024;
    const int OldSize = 4*10*1024;
    _young[0].reserve(YoungSize);
    _young[1].reserve(YoungSize);
    _old.reserve(OldSize); // ~1MB
    _current = 0;
}
gcimpl::~gcimpl() {
    collect();
}
void gcimpl::collect() { // collect young gen
    const size_t OldLimit = 4*9*1024;
    std::cerr << "+gc: " << _young[0].size() << " " << _young[1].size() << " " << _old.size() << std::endl;

    // mark live cells
    std::for_each(_roots.begin(), _roots.end(), [=](cell** root) {
            this->mark(*root);
        });

    _ptrs.clear();

    // if over threshold for oldgen, collect it
    const bool collecting_old = _old.size() > OldLimit;
    if (collecting_old)
        oldgen_collect();

    int dead = _current, live = _current ? 0 : 1;

    // paranoia...
    assert(_young[live].size() == 0);

    // now move live cells to live heap
    // fixup pointers and unmark as we go
    // clear all references from dead heap
    // keep a log of all moves separately?
    // for pointer fixing: map oldptr -> newptr
    // when fixing up, find oldptr and set it to newptr

    // There is a better way to do this which is faster
    // and doesn't require _ptrs.. but it's more difficult.
    // will implement later.
    size_t oldend = _old.size();
    std::for_each(_young[dead].begin(), _young[dead].end(), [&](cell& c) {
            if (c.flags & cell::GcMark) {
                c.flags &= ~cell::GcMark;
                if (c.flags & cell::GcSurvivor1) {
                    c.flags = (c.flags & ~cell::GcSurvivor1) | cell::GcSurvivor2;
                }
                else if (c.flags & cell::GcSurvivor2) {
                    c.flags |= cell::GcOld;
                }
                else {
                    c.flags |= cell::GcSurvivor1;
                }
                if (!(c.flags & cell::GcOld)) {
                    _young[live].emplace_back(std::move(c));
                    _ptrs.emplace(&c, &_young[live].back());
                }
                else {
                    _old.emplace_back(std::move(c));
                    _ptrs.emplace(&c, &_old.back());
                }
                // what has happened to the old cell?
            }
        });

    auto ptrs_e = _ptrs.end();

    // pointer fixup and unmark in roots
    std::for_each(_roots.begin(), _roots.end(), [&](cell** cpp) {
            auto it = _ptrs.find(*cpp);
            if (it != ptrs_e)
                *cpp = it->second;
        });

    // pointer fixup in _young[live]
    std::for_each(_young[live].begin(), _young[live].end(), [&](cell& c) {
            if (c.typa == cell::Cell) {
                auto it = _ptrs.find(c.a.c);
                if (it != ptrs_e)
                    c.a.c = it->second;
            }
            if (c.typb == cell::Cell) {
                auto it = _ptrs.find(c.b.c);
                if (it != ptrs_e)
                    c.b.c = it->second;
            }
        });

    // pointer fixup and unmark in oldgen
    std::for_each(_old.begin(), _old.end(), [&](cell& c) {
            c.flags &= ~cell::GcMark; // unmark!
            if (c.typa == cell::Cell) {
                auto it = _ptrs.find(c.a.c);
                if (it != ptrs_e)
                    c.a.c = it->second;
            }
            if (c.typb == cell::Cell) {
                auto it = _ptrs.find(c.b.c);
                if (it != ptrs_e)
                    c.b.c = it->second;
            }
        });

    // do we need to do more here to ensure that
    // we aren't hanging on to value references?
    _young[dead].clear();

    // btw, unmarking/pointerfixing all marked cells in oldgen
    // (can this step be avoided somehow?)
    // (keep a registry of oldgen cells that
    // reference younggen cells, unmark those
    // here perhaps...)

    // switch heaps
    _current = live;

    std::cerr << "-gc: " << _young[0].size() << " " << _young[1].size() << " " << _old.size() << std::endl;
}

namespace {
    inline bool marked(const cell& c) {
        return c.flags & cell::GcMark;
    }
}

void gcimpl::oldgen_collect() {
    auto bottom = _old.begin();
    auto top = _old.end();
    size_t swaps = 0;
    while (true) {
        while (bottom != top && !marked(*(--top)))
            ;
        while (bottom != top && marked(*bottom))
            ++bottom;

        if (bottom == top)
            break;

        // top is now pointing to a live cell at the top of the heap
        // bottom is pointing to a dead cell at the bottom of the heap
        // swap them
        _ptrs.emplace(&(*top), &(*bottom));
        std::swap(*bottom, *top);
        ++swaps;
        // top = dead cell at top of heap
        // bottom = live cell at bottom of heap
        // go on then..
        // could check for time here and only
        // keep swapping for a certain time...
    }
    // remove the dead cells from the heap
    _old.erase(_old.end()-swaps, _old.end());

    // todo: if necessary, grow the heap
    // growing the heap will require major
    // pointer fixup...
}

void gcimpl::mark(cell* c) {
    while (!(c->flags & cell::GcMark)) {
        c->flags |= cell::GcMark;
        if (c->typa == cell::Cell && c->a.c)
            mark(c->a.c);
        if (c->typb == cell::Cell && c->b.c)
            c = c->b.c;
        else
            break;
    }
}
void gcimpl::addroot(cell** c) {
    _roots.insert(c);
}
void gcimpl::rmroot(cell** c) {
    _roots.erase(c);
}
cell* gcimpl::alloc(size_t len) {
    assert(len > 0 && len < _young[_current].capacity());
    if (_young[_current].capacity() < _young[_current].size() + len)
        collect();
    const size_t nowsize = _young[_current].size();
    _young[_current].resize(nowsize+len);
    return &_young[_current][nowsize];
}

void gcimpl::stop_the_world() {
    // todo
}

void gcimpl::start_the_world() {
    // todo
}

void gcimpl::expand_heap() {
    // todo
    // save old base address and bounds of heap
    // resize heap
    // get new base address and bounds of heap
    // run through heap and roots and adjust all pointers (pointers that go outside of old heap are not changed)
    throw std::runtime_error("out of cell space");
}


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

/*
struct value_mgr {
    template <typename T, typename... Args>
    T* create(Args&& args) {
        T* val = new T(std::forward(args)...);
        save_value(val, sizeof(T));
        return val;
    }

    void save_value(value* v, size_t sz);
    void collect();
};
*/

#include <iostream>

std::ostream& print(std::ostream& s, cell* c) {
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
        case cell::Cell: print(s, c); break;
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

int main() {

    cell* a = cellGC.alloc(1);
    cell* b = cellGC.alloc(2);
    cell* c = cellGC.alloc(3);

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

    print(std::cout, a) << "\n";
    print(std::cout, (c+2)) << "\n";

    // TODO: fix!
    // have to pass pointers to the roots so we can fix them up ofc...
    cellGC.addroot(&a);
    cellGC.addroot(&b);
    cellGC.addroot(&c);

    cellGC.collect();

    print(std::cout, a) << "\n";

    cellGC.rmroot(&a);
    cellGC.rmroot(&b);
    cellGC.rmroot(&c);
}
