#pragma once

#include <memory>

namespace util {
    template <typename T>
    struct intrusive_ptr {
        typedef T element_type;

        typedef T * intrusive_ptr::*boolean_type;

        intrusive_ptr() : _ptr(0) {
        }

        explicit intrusive_ptr(T* ptr) : _ptr(ptr) {
            if (_ptr)
                incref(_ptr);
        }

        intrusive_ptr(const intrusive_ptr& p) : _ptr(0) {
            *this = p;
        }

        intrusive_ptr(intrusive_ptr&& p) : _ptr(0) {
            *this = std::move(p);
        }

        intrusive_ptr& operator=(intrusive_ptr&& p) {
            if (this != &p) {
                T* tmp = _ptr;
                _ptr = p._ptr;
                p._ptr = 0;
                if (tmp && (tmp == _ptr))
                    decref(_ptr);
            }
            return *this;
        }

        ~intrusive_ptr() {
            if (_ptr)
                decref(_ptr);
        }

        intrusive_ptr& operator=(const intrusive_ptr& p) {
            T* tmp = _ptr;
            _ptr = p._ptr;
            if (_ptr) incref(_ptr);
            if (tmp) decref(tmp);
            return *this;
        }

        T* operator->() {
            return _ptr;
        }

        const T* operator->() const {
            return _ptr;
        }

        T& operator*() {
            return *_ptr;
        }

        const T& operator*() const {
            return *_ptr;
        }

        T* get() {
            return _ptr;
        }

        const T* get() const {
            return _ptr;
        }

        bool operator==(const T* o) const {
            return _ptr == o;
        }

        bool operator!=(const T* o) const {
            return _ptr != o;
        }

        operator boolean_type () const {
            return _ptr != 0 ? &intrusive_ptr::_ptr : 0;
        }

        bool operator! () const {
            return _ptr == 0;
        }

        T* _ptr;
    };
}
