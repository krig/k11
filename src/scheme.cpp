// Fun with variadic templates and boost::variant

#include <boost/variant.hpp>
#include <boost/variant/apply_visitor.hpp>
#include <boost/variant/static_visitor.hpp>
#include <boost/lexical_cast.hpp>
#include <vector>
#include <string>
#include <sstream>

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

int main() {
    list l = make_list(atom(3), atom("hello world"), make_list(atom(6), atom(9.3f)));
    list l2 = make_list(l, l);

    std::string s1 = boost::apply_visitor(list_stringer(), l);
    std::string s2 = boost::apply_visitor(list_stringer(), l2);
    printf("%s\n%s\n", s1.c_str(), s2.c_str());
}
