#pragma once

#include "intrusive_ptr.hpp"
#include "symbol.hpp"
#include <iostream>

struct value;
typedef util::intrusive_ptr<value> value_ptr;

struct nil_cell {};

struct cell {
    // mark = in gc, survivor1 = survived 1 younggen collection
    // survivor2 = survived 2 younggen collections, old = in oldgen
    // symbols are interned strings. Interning is handled elsewhere
    enum Flag { GcMark = 1, GcSurvivor1 = 2, GcSurvivor2 = 4, GcOld = 8 };
    enum Typ { Nil, Int, Float, Double, Cell, Value, String, Symbol };
    union slot {
        slot() : n() {}
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

    cell();
    cell(const cell& c);
    cell(cell&& c);
    ~cell();
    cell& operator=(const cell& c);
    cell& operator=(cell&& c);
    void destroy();
    void destroycar();
    void destroycdr();
    void car(int i);
    void car(float f);
    void car(double d);
    void car(std::string&& s);
    void car(value_ptr&& v);
    void car(cell* c);
    void car(const symbol& sym);
    void nil_car();
    void cdr(int i);
    void cdr(float f);
    void cdr(double d);
    void cdr(std::string&& s);
    void cdr(value_ptr&& v);
    void cdr(cell* c);
    void cdr(const symbol& sym);
    void nil_cdr();
    bool car_is_nil() const;
    bool cdr_is_nil() const;

    slot a;
    slot b;
    uint16_t flags;
    uint8_t typa;
    uint8_t typb;
};



inline
cell::cell() : flags(0), typa(Nil), typb(Nil) {
}

inline
cell::cell(const cell& c) {
    *this = c;
}

inline
cell::cell(cell&& c) : flags(0), typa(Nil), typb(Nil) {
    *this = std::move(c);
}

inline
cell::~cell() {
    destroy();
}

inline
bool cell::car_is_nil() const {
    return typa == Nil;
}

inline
bool cell::cdr_is_nil() const {
    return typb == Nil;
}

template <typename T> const T* car(const cell* c);
template <typename T> const T* cdr(const cell* c);
template <typename T> T* car(cell* c);
template <typename T> T* cdr(cell* c);

template <> inline const int* car<int>(const cell* c) { return c->typa==cell::Int?&c->a.i:nullptr; }
template <> inline const float* car<float>(const cell* c) { return c->typa==cell::Float?&c->a.f:nullptr; }
template <> inline const double* car<double>(const cell* c) { return c->typa==cell::Double?&c->a.d:nullptr; }
template <> inline const std::string* car<std::string>(const cell* c) { return c->typa==cell::String?&c->a.s:nullptr; }
template <> inline const value_ptr* car<value_ptr>(const cell* c) { return c->typa==cell::Value?&c->a.v:nullptr; }
template <> inline const symbol* car<symbol>(const cell* c) { return c->typa==cell::Symbol?&c->a.y:nullptr; }
template <> inline const cell* car<cell>(const cell* c) { return c->typa==cell::Cell?c->a.c:nullptr; }
template <> inline const int* cdr<int>(const cell* c) { return c->typb==cell::Int?&c->b.i:nullptr; }
template <> inline const float* cdr<float>(const cell* c) { return c->typb==cell::Float?&c->b.f:nullptr; }
template <> inline const double* cdr<double>(const cell* c) { return c->typb==cell::Double?&c->b.d:nullptr; }
template <> inline const std::string* cdr<std::string>(const cell* c) { return c->typb==cell::String?&c->b.s:nullptr; }
template <> inline const value_ptr* cdr<value_ptr>(const cell* c) { return c->typb==cell::Value?&c->b.v:nullptr; }
template <> inline const symbol* cdr<symbol>(const cell* c) { return c->typb==cell::Symbol?&c->b.y:nullptr; }
template <> inline const cell* cdr<cell>(const cell* c) { return c->typb==cell::Cell?c->b.c:nullptr; }
template <> inline int* car<int>(cell* c) { return c->typa==cell::Int?&c->a.i:nullptr; }
template <> inline float* car<float>(cell* c) { return c->typa==cell::Float?&c->a.f:nullptr; }
template <> inline double* car<double>(cell* c) { return c->typa==cell::Double?&c->a.d:nullptr; }
template <> inline std::string* car<std::string>(cell* c) { return c->typa==cell::String?&c->a.s:nullptr; }
template <> inline value_ptr* car<value_ptr>(cell* c) { return c->typa==cell::Value?&c->a.v:nullptr; }
template <> inline symbol* car<symbol>(cell* c) { return c->typa==cell::Symbol?&c->a.y:nullptr; }
template <> inline cell* car<cell>(cell* c) { return c->typa==cell::Cell?c->a.c:nullptr; }
template <> inline int* cdr<int>(cell* c) { return c->typb==cell::Int?&c->b.i:nullptr; }
template <> inline float* cdr<float>(cell* c) { return c->typb==cell::Float?&c->b.f:nullptr; }
template <> inline double* cdr<double>(cell* c) { return c->typb==cell::Double?&c->b.d:nullptr; }
template <> inline std::string* cdr<std::string>(cell* c) { return c->typb==cell::String?&c->b.s:nullptr; }
template <> inline value_ptr* cdr<value_ptr>(cell* c) { return c->typb==cell::Value?&c->b.v:nullptr; }
template <> inline symbol* cdr<symbol>(cell* c) { return c->typb==cell::Symbol?&c->b.y:nullptr; }
template <> inline cell* cdr<cell>(cell* c) { return c->typb==cell::Cell?c->b.c:nullptr; }

std::ostream& operator<<(std::ostream& s, cell* c);
