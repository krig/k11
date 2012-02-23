// Fun with variadic templates and boost::variant
// compile with gcc 4.7+
// /opt/bin/g++-4.7 -std=c++0x -I/opt/include -o scheme scheme.cpp

#include <boost/variant.hpp>
#include <boost/variant/apply_visitor.hpp>
#include <boost/variant/static_visitor.hpp>
#include <boost/lexical_cast.hpp>
#include <vector>
#include <string>
#include <sstream>
#include <stdexcept>
#include <stdlib.h>
#include "print.hpp"

class symbol : public std::string {
public:
    symbol() : std::string() {
    }
    symbol(const char* a, const char* b) : std::string(a, b) {
    }
    explicit symbol(const char* s) : std::string(s) {
    }
    explicit symbol(const std::string& o) : std::string(o) {
    }
};

typedef boost::variant<int, float, std::string, symbol> atom;
typedef boost::make_recursive_variant<atom, std::vector<boost::recursive_variant_>>::type sexpr;

void pushbacker(std::vector<sexpr>& v, int a) {
    v.push_back(atom(a));
}

void pushbacker(std::vector<sexpr>& v, float a) {
    v.push_back(atom(a));
}

void pushbacker(std::vector<sexpr>& v, const symbol& a) {
    v.push_back(atom(a));
}

void pushbacker(std::vector<sexpr>& v, const std::string& a) {
    v.push_back(atom(a));
}

void pushbacker(std::vector<sexpr>& v, const atom& a) {
    v.push_back(a);
}

void pushbacker(std::vector<sexpr>& v, const sexpr& l) {
    v.push_back(l);
}

template <class T, typename... Ts>
void pushbacker(std::vector<sexpr>& v, const T& t, const Ts&... ts) {
    v.push_back(t);
    pushbacker(v, ts...);
}

template <typename... Ts>
sexpr list(const Ts&... ts) {
    std::vector<sexpr> v;
    v.reserve(sizeof...(ts));
    pushbacker(v, ts...);
    return sexpr(v);
}

class atom_stringer : public boost::static_visitor<std::string>
{
public:
    std::string operator()(const std::string& value) const {
        std::stringstream ss;
        ss << '"' << value << '"';
        return ss.str();
    }

    std::string operator()(const symbol& value) const {
        return value;
    }

    std::string operator()(int value) const {
        return boost::lexical_cast<std::string>(value);
    }

    std::string operator()(float value) const {
        return boost::lexical_cast<std::string>(value);
    }
};

class list_stringer : public boost::static_visitor<std::string>
{
public:

    std::string operator()(const atom& a) const
    {
        return boost::apply_visitor(atom_stringer(), a);
    }

    std::string operator()(const std::vector<sexpr>& v) const
    {
        std::stringstream ss;
        ss << "(";
        bool first = true;
        for (std::vector<sexpr>::const_iterator i = v.begin(); i != v.end(); ++i) {
            if (first)
                first = false;
            else
                ss << " ";
            ss << boost::apply_visitor(list_stringer(), *i);
        }
        ss << ")";
        return ss.str();
    }

};

std::string to_str(const sexpr& l) {
    return boost::apply_visitor(list_stringer(), l);
}

std::string to_str(const atom& a) {
    return boost::apply_visitor(atom_stringer(), a);
}

struct streamptr {
    const char* _s;
    explicit streamptr(const char* s) : _s(s) {
    }
    char peek() {
        while (isspace(*_s))
            ++_s;
        return *_s;
    }
    bool eof() const {
        return *_s == 0;
    }
    char next() {
        while (isspace(*_s))
            ++_s;
        return *_s++;
    }

    atom read_string() {
        next();
        const char* sb = _s;
        const char* se = strchr(sb, '"');
        _s = se+1;
        return std::string(sb, se);
    }

    atom read_number() {
        bool flt = false;
        const char* n = _s;
        const char* e = _s;
        while (isdigit(*e) || *e == '.') {
            if (*e == '.')
                flt = true;
            ++e;
        }
        _s = e;
        std::string sn(n, e);
        if (flt) {
            return boost::lexical_cast<float>(sn);
        }
        else {
            return boost::lexical_cast<int>(sn);
        }
    }
    atom read_symbol() {
        const char* s = _s;
        const char* e = s;
        while (*e != ')' && *e != '\0' && !isspace(*e))
            ++e;
        _s = e;
        return atom(symbol(s, e));
    }
};

sexpr readref(streamptr& s) {
    if (s.peek() == '(') {
        std::vector<sexpr> v;
        s.next();
        while (s.peek() != ')')
            v.push_back(readref(s));
        s.next();
        return v;
    }
    else if (s.peek() == ')') {
        throw std::runtime_error("mismatched )");
    }
    else if (s.peek() == '"') {
        return s.read_string();
    }
    else if (isdigit(s.peek())) {
        return s.read_number();
    }
    else if (s.peek() == '\'') {
        s.next();
        return list(atom(symbol("quote")), readref(s));
    }
    else {
        return s.read_symbol();
    }
}

sexpr read(const char* s) {
    streamptr sp(s);
    return readref(sp);
}

int main() {
    using namespace print;

    sexpr l1 = list(atom(3), atom("hello world"), list(atom(6), atom(9.3f)));
    sexpr l2 = read("(fn (x) (1 2 3 \"wee\" '(4 5 6)))");
    sexpr l3 = list(l1, l2);
    sexpr l4 = list(symbol("def"), symbol("z"), list(1, 2, "hello", list(7, 9)));

    std::string s1 = to_str(l1);
    std::string s2 = to_str(l2);
    std::string s3 = to_str(l3);

    pn("sizeof %d", sizeof(l3));
    pn("%s\n%s\n%s\n%s", s1, s2, s3, to_str(l4));
}
