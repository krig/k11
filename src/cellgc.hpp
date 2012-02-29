#pragma once

#include <cstddef>


struct cell;
struct gcimpl;

struct cellgc {
    cellgc();
    ~cellgc();
    void collect();
    void addroot(cell** c);
    void rmroot(cell** c);
    cell* alloc(size_t len = 1);
    cell* alloc_list(size_t len);

    gcimpl* _impl;

    static cellgc& instance();
};
