#include <iostream>
#include <algorithm>

struct xrange_t {
    struct iterator {
        iterator() : _i(0) { }
        explicit iterator(int i) : _i(i) { }
        int operator*() const { return _i; }
        iterator& operator++() { ++_i; return *this; }
        iterator operator++(int) { iterator t(_i); ++_i; return t; }
        bool operator==(const iterator& i) const { return _i == i._i; }
        bool operator!=(const iterator& i) const { return _i != i._i; }

        int _i;
    };

    struct reverse_iterator {
        reverse_iterator() : _i(0) { }
        explicit reverse_iterator(int i) : _i(i) { }
        int operator*() const { return _i; }
        reverse_iterator& operator++() { --_i; return *this; }
        reverse_iterator operator++(int) { reverse_iterator t(_i); --_i; return t; }
        bool operator==(const reverse_iterator& i) const { return _i == i._i; }
        bool operator!=(const reverse_iterator& i) const { return _i != i._i; }

        int _i;
    };

    xrange_t(int lo, int hi) : _l(lo), _h(hi) {
    }

    iterator begin() const {
        return iterator(_l);
    }

    iterator end() const {
        return iterator(_h);
    }

    reverse_iterator rbegin() const {
        return reverse_iterator(_h-1);
    }

    reverse_iterator rend() const {
        return reverse_iterator(_l-1);
    }

    int _l, _h;
};

struct xrrange_t : private xrange_t {
    explicit xrrange_t(const xrange_t& r) : xrange_t(r._l, r._h) {}
    reverse_iterator begin() const {
        return rbegin();
    }
    reverse_iterator end() const {
        return rend();
    }
};

auto range(int lo, int hi) -> xrange_t {
    return xrange_t(lo, hi);
}

auto reverse(const xrange_t& r) -> xrrange_t {
    return xrrange_t(r);
}

int main() {
    using namespace std;
    for (auto i : range(0, 10))
        cout << i << " ";
    cout << endl;
    for (auto i : range(-10, 0))
        cout << i << " ";
    cout << endl;
    for (auto i : reverse(range(0, 10)))
        cout << i << " ";
    cout << endl;
    for (auto i : reverse(range(-10, 0)))
        cout << i << " ";
    cout << endl;
}
