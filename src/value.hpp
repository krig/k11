#pragma once

struct value {
    virtual ~value() {}
    virtual const char* name() const = 0;
    virtual void incref() = 0;
    virtual void decref() = 0;
};

inline void incref(value* v) { v->incref(); }
inline void decref(value* v) { v->decref(); }

