#include "cellgc.hpp"
#include "cell.hpp"

#include <set>
#include <vector>
#include <unordered_set>
#include <unordered_map>
#include <algorithm>
#include <cassert>
#include <stdexcept>

struct gcimpl {
    gcimpl();
    ~gcimpl();
    void collect(); // younggen collection
    void addroot(cell** c);
    void rmroot(cell** c);
    cell* alloc(size_t len);
    cell* alloc_list(size_t len);

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
cell* cellgc::alloc_list(size_t len) { return _impl->alloc_list(len); }

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
    //std::cerr << "gc: young(" << _young[0].size() << "/" << _young[1].size() << "), old(" << _old.size() << ")\n";

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

    // pointer fixup can be done asynchronously and simultaneously
    // across the three locations (roots, young, old)
    // adding it is (semi)trivial: spin off each for_each in an std::async()
    // and catch the futures below

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

    //std::cerr << "gc: -> young(" << _young[0].size() << "/" << _young[1].size() << "), old(" << _old.size() << ")" << std::endl;
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

cell* gcimpl::alloc_list(size_t len) {
    cell* c = alloc(len);
    cell* p = c;
    --len;
    while (len) {
        p->cdr(p+1);
        p = p+1;
        --len;
    }
    p->nil_cdr();
    return c;
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
