#pragma once

#include <set>
#include <string>

struct symbol {
    typedef std::set<std::string> table_type;
    static table_type table;

    explicit symbol(const char* s) {
        i = table.insert(std::move(std::string(s))).first;
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
