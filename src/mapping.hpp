#pragma once

// provides iteration adapters for map types
// whose iterators return std::pair<key, value>
// to allow direct iteration of keys and values

namespace mapping
{
    inline namespace internal {
    template <typename M, typename I, int Index>
    struct const_mapdapter {
        struct iterator {
            typename M::const_iterator _i;
            iterator() {}
            iterator(const iterator& i) : _i(i._i) {}
            iterator(const typename M::const_iterator& mi) : _i(mi) {}
            iterator& operator=(const iterator& i) {
                _i = i._i; return *this; }
            iterator& operator++() {
                ++_i; return *this; }
            iterator operator++(int) {
                iterator k(_i++); return k; }
            const I& operator*() const {
                return std::get<Index>(*_i); }
            const I* operator->() const {
                return &std::get<Index>(*_i); }
            bool operator==(const iterator& i) const {
                return _i == i._i; }
            bool operator!=(const iterator& i) const {
                return _i != i._i; }
        };
        const_mapdapter(const M& m) : _m(m) { }
        iterator begin() const { return iterator(_m.begin()); }
        iterator end() const { return iterator(_m.end()); }
    private:
        const M& _m;
    };
    template <typename M, typename I, int Index>
    struct mapdapter {
        struct iterator {
            typename M::iterator _i;
            iterator() {}
            iterator(const iterator& i) : _i(i._i) {}
            iterator(const typename M::iterator& mi) : _i(mi) {}
            iterator& operator=(const iterator& i) {
                _i = i._i; return *this; }
            iterator& operator++() {
                ++_i; return *this; }
            iterator operator++(int) {
                iterator k(_i++); return k; }
            I& operator*() const {
                return std::get<Index>(*_i); }
            I* operator->() const {
                return &std::get<Index>(*_i); }
            bool operator==(const iterator& i) const {
                return _i == i._i; }
            bool operator!=(const iterator& i) const {
                return _i != i._i; }
        };
        mapdapter(M* m) : _m(m) { }
        iterator begin() const { return iterator(_m->begin()); }
        iterator end() const { return iterator(_m->end()); }
    private:
        M* _m;
    };
    }

    template<typename M>
    auto keys(const M& m) -> decltype(const_mapdapter<M, typename M::key_type, 0>(m)) {
        return const_mapdapter<M, typename M::key_type, 0>(m);
    }

    template<typename M>
    auto values(const M& m) -> decltype(const_mapdapter<M, typename M::mapped_type, 1>(m)) {
        return const_mapdapter<M, typename M::mapped_type, 1>(m);
    }

    template<typename M>
    auto values(M& m) -> decltype(mapdapter<M, typename M::mapped_type, 1>(&m)) {
        return mapdapter<M, typename M::mapped_type, 1>(&m);
    }

}
