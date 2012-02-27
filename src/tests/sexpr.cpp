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

// cells are GC:d so use naked pointers

struct nil_cell {};

struct cell {
    // mark = in gc, survivor1 = survived 1 younggen collection
    // survivor2 = survived 2 younggen collections, old = in oldgen
    enum Flag { GcMark = 1, GcSurvivor1 = 2, GcSurvivor2 = 4, GcOld = 8 };
    enum Typ { Int, Float, Double, Cell, Value, String, Nil };
    union slot {
        slot() : i(0) {}
        ~slot() {}
        int i;
        float f;
        double d;
        cell* c;
        value_ptr v;
        std::string s;
        nil_cell n;
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
            case Int: a.i = c.a.i; break;
            case Float: a.f = c.a.f; break;
            case Double: a.d = c.a.d; break;
            case Cell: a.c = c.a.c; break;
            case Value: a.v = c.a.v; break;
            case String: a.s = c.a.s; break;
            case Nil: break;
            }
        }
        else {
            destroy();
            switch (typa = c.typa) {
            case Int: a.i = c.a.i; break;
            case Float: a.f = c.a.f; break;
            case Double: a.d = c.a.d; break;
            case Cell: a.c = c.a.c; break;
            case Value: new (&a.v) value_ptr(c.a.v); break;
            case String: new (&a.s) std::string(c.a.s); break;
            case Nil: break;
            }
        }
        if (typb == c.typb) {
            switch (c.typb) {
            case Int: b.i = c.b.i; break;
            case Float: b.f = c.b.f; break;
            case Double: b.d = c.b.d; break;
            case Cell: b.c = c.b.c; break;
            case Value: b.v = c.b.v; break;
	        case String: b.s = c.b.s; break;
            case Nil: break;
            }
        }
        else {
            switch (typb = c.typb) {
            case Int: b.i = c.b.i; break;
            case Float: b.f = c.b.f; break;
            case Double: b.d = c.b.d; break;
            case Cell: b.c = c.b.c; break;
            case Value: new (&b.v) value_ptr(c.b.v); break;
	        case String: new (&b.s) std::string(c.b.s); break;
            case Nil: break;
            }
        }
        flags = c.flags;
        return *this;
    }

    cell& operator=(cell&& c) {
        if (this != &c) {
            destroy();

            switch (c.typa) {
            case Int: a.i = c.a.i; break;
            case Float: a.f = c.a.f; break;
            case Double: a.d = c.a.d; break;
            case Cell: a.c = c.a.c; break;
	        case Value: new (&a.v) value_ptr(std::move(c.a.v)); break;
	        case String: new (&a.s) std::string(std::move(c.a.s)); break;
            case Nil: break;
            }
            switch (c.typb) {
	        case Int: b.i = c.b.i; break;
	        case Float: b.f = c.b.f; break;
	        case Double: b.d = c.b.d; break;
	        case Cell: b.c = c.b.c; break;
	        case Value: new (&b.v) value_ptr(std::move(c.b.v)); break;
	        case String: new (&b.s) std::string(std::move(c.b.s)); break;
	        case Nil: break;
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
            new (&a.s) std::string(s);
        }
        else {
            a.s = s;
        }
    }

    void car(cell* c) {
        if (typa != Cell) {
            destroycar();
            typa = Cell;
        }
        a.c = c;
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
            new (&b.s) std::string(s);
        }
        else {
            b.s = s;
        }
    }

    void cdr(cell* c) {
        if (typb != Cell) {
            destroycdr();
            typb = Cell;
        }
        b.c = c;
    }

    void nil_cdr() {
        if (typb != Nil) {
            destroycdr();
            typb = Nil;
        }
    }

    slot a;
    slot b;
    uint16_t flags;
    uint8_t typa;
    uint8_t typb;
};

template <typename T>
const T* car(const cell& c);
template <typename T>
const T* cdr(const cell& c);

template <> const int* car<int>(const cell& c) { return c.typa==cell::Int?&c.a.i:nullptr; }
template <> const float* car<float>(const cell& c) { return c.typa==cell::Float?&c.a.f:nullptr; }
template <> const double* car<double>(const cell& c) { return c.typa==cell::Double?&c.a.d:nullptr; }
template <> const std::string* car<std::string>(const cell& c) { return c.typa==cell::String?&c.a.s:nullptr; }
template <> const value_ptr* car<value_ptr>(const cell& c) { return c.typa==cell::Value?&c.a.v:nullptr; }

template <> const int* cdr<int>(const cell& c) { return c.typb==cell::Int?&c.b.i:nullptr; }
template <> const float* cdr<float>(const cell& c) { return c.typb==cell::Float?&c.b.f:nullptr; }
template <> const double* cdr<double>(const cell& c) { return c.typb==cell::Double?&c.b.d:nullptr; }
template <> const std::string* cdr<std::string>(const cell& c) { return c.typb==cell::String?&c.b.s:nullptr; }
template <> const value_ptr* cdr<value_ptr>(const cell& c) { return c.typb==cell::Value?&c.b.v:nullptr; }


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
    void addroot(cell* c);
    void rmroot(cell* c);
    cell* alloc(size_t len = 1);

    gcimpl* _impl;
} cellGC;

struct gcimpl {
    gcimpl();
    ~gcimpl();
    void collect(); // younggen collection
    void addroot(cell* c);
    void rmroot(cell* c);
    cell* alloc(size_t len);

    void expand_heap();
    void mark_young(cell* c);

    // this would be needed if multithreaded
    void stop_the_world();
    void start_the_world();

    std::unordered_set<cell*> _roots;
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
void cellgc::addroot(cell* c) { _impl->addroot(c); }
void cellgc::rmroot(cell* c) { _impl->rmroot(c); }
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
    // TODO: if oldgen is reaching limit, collect old gen
    std::cerr << "+gc: " << _young[0].size() << " " << _young[1].size() << " " << _old.size() << std::endl;

    // mark live cells in young heap
    std::for_each(_roots.begin(), _roots.end(), [=](cell* root) {
            this->mark_young(root);
        });

    int dead = _current, live = _current ? 0 : 1;

    cell* dead_heap = &_young[dead].front();
    cell* live_heap = &_young[live].front();

    // paranoia...
    assert(_young[live].size() == 0);

    // now move live cells to live heap
    // fixup pointers and unmark as we go
    // clear all references from dead heap
    // keep a log of all moves separately?
    // for pointer fixing: map oldptr -> newptr
    // when fixing up, find oldptr and set it to newptr
    size_t oldend = _old.size();
    _ptrs.clear();
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

    // pointer fixup in _young[live]
    auto ptrs_e = _ptrs.end();
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

void gcimpl::mark_young(cell* c) {
    while (!(c->flags & cell::GcMark)) {
        c->flags |= cell::GcMark;
        if (c->typa == cell::Cell && c->a.c)
            mark_young(c->a.c);
        if (c->typb == cell::Cell && c->b.c)
            c = c->b.c;
        else
            break;
    }
}
void gcimpl::addroot(cell* c) {
    _roots.insert(c);
}
void gcimpl::rmroot(cell* c) {
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
    proc_value() : value_impl("proc") { cellGC.addroot(expr); /* add env cell references as roots */ }
    ~proc_value() { cellGC.rmroot(expr); }
    value* _parent;
    cell* expr;
    cell::slot env[Nslots];
    uint16_t envt[Nslots];
};

struct file_value : public value_impl {
    file_value() : value_impl("file"), _file(0) {}
    virtual ~file_value() { if (_file) fclose(_file); }
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

int main() {

    cell* a = cellGC.alloc(1);
    cell* b = cellGC.alloc(1);
    cell* c = cellGC.alloc(3);
    c->car("hello");
    c->cdr(b);
    b->car(1);
    b->cdr(c);
    c->car(a);
    c->cdr(c+1);
    (c+1)->car("world");
    (c+1)->nil_cdr();
    (c+2)->car(3.0);
    (c+2)->nil_cdr();

    cellGC.addroot(a);
    cellGC.addroot(b);
    cellGC.addroot(c);

    cellGC.collect();

    cellGC.rmroot(a);
    cellGC.rmroot(b);
    cellGC.rmroot(c);
}
