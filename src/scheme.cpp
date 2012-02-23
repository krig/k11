// Fun with variadic templates and boost::variant

#include <boost/variant.hpp>
#include <boost/variant/apply_visitor.hpp>
#include <boost/variant/static_visitor.hpp>
#include <boost/lexical_cast.hpp>
#include <vector>
#include <string>
#include <sstream>
#include <stdexcept>
#include <stdlib.h>

typedef boost::variant<int, float, std::string> atom;
typedef boost::make_recursive_variant<atom, std::vector<boost::recursive_variant_>>::type list;

void pushbacker(std::vector<list>& v, const atom& a) {
    v.push_back(a);
}

void pushbacker(std::vector<list>& v, const list& l) {
    v.push_back(l);
}

template <class T, typename... Ts>
void pushbacker(std::vector<list>& v, const T& t, const Ts&... ts) {
    v.push_back(t);
    pushbacker(v, ts...);
}

template <typename... Ts>
list make_list(const Ts&... ts) {
    std::vector<list> v;
    pushbacker(v, ts...);
    return list(v);
}

class atom_stringer : public boost::static_visitor<std::string>
{
public:
    std::string operator()(const std::string& value) const {
        std::stringstream ss;
        ss << '"' << value << '"';
        return ss.str();
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

    std::string operator()(const std::vector<list>& v) const
    {
        std::stringstream ss;
        ss << "(";
        bool first = true;
        for (std::vector<list>::const_iterator i = v.begin(); i != v.end(); ++i) {
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

std::string list_to_string(const list& l) {
    return boost::apply_visitor(list_stringer(), l);
}

std::string atom_to_string(const atom& a) {
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
};

list readref(streamptr& s) {
    if (s.peek() == '(') {
        std::vector<list> v;
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
    else {
        throw std::runtime_error("unexpected state");
    }
}

list read(const char* s) {
    streamptr buf(s);
    return readref(buf);
}

int main() {
    list l = make_list(atom(3), atom("hello world"), make_list(atom(6), atom(9.3f)));
    list l2 = make_list(l, l);
    list l3 = read("(1 2 3 \"4\" (4 5 6))");

    std::string s1 = list_to_string(l);
    std::string s2 = list_to_string(l2);
    std::string s3 = list_to_string(l3);
    printf("%s\n%s\n%s\n", s1.c_str(), s2.c_str(), s3.c_str());
}
