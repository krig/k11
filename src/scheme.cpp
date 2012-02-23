// Fun with variadic templates and boost::variant
// an implementation of http://norvig.com/lispy.html in C++11 (sort of)
// compile with gcc 4.7+
// g++-4.7 -std=c++0x -o scheme scheme.cpp
// or clang (HEAD)
// clang++ -std=c++0x -o scheme scheme.cpp

#include <boost/variant.hpp>
#include <boost/variant/apply_visitor.hpp>
#include <boost/variant/static_visitor.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/function.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/any.hpp>
#include <vector>
#include <map>
#include <string>
#include <sstream>
#include <stdexcept>
#include <iostream>
#include <stdlib.h>

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

typedef boost::variant<double, std::string, symbol> atom;
typedef boost::make_recursive_variant<atom,
                                      std::vector<boost::recursive_variant_>,
                                      boost::function1<boost::recursive_variant_, const boost::any&>>::type sexpr;
typedef std::vector<sexpr> sexprs;
typedef boost::function1<sexpr, const boost::any&> lambda;

void vec_arg(sexprs& v, int a) {
    v.push_back(atom((double)a));
}

void vec_arg(sexprs& v, double a) {
    v.push_back(atom(a));
}

void vec_arg(sexprs& v, const symbol& a) {
    v.push_back(atom(a));
}

void vec_arg(sexprs& v, const std::string& a) {
    v.push_back(atom(a));
}

void vec_arg(sexprs& v, const sexpr& l) {
    v.push_back(l);
}

template <class T, typename... Ts>
void vec_arg(sexprs& v, const T& t, const Ts&... ts) {
    v.push_back(t);
    vec_arg(v, ts...);
}

template <typename... Ts>
sexpr list(const Ts&... ts) {
    sexprs v;
    v.reserve(sizeof...(ts));
    vec_arg(v, ts...);
    return sexpr(v);
}

class atom2s : public boost::static_visitor<std::string>
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

    std::string operator()(double value) const {
        char tmp[80];
        snprintf(tmp, sizeof(tmp), "%.6g", value);
        return tmp;
    }
};

class sexpr2s : public boost::static_visitor<std::string>
{
public:

    std::string operator()(const atom& a) const {
        return boost::apply_visitor(atom2s(), a);
    }

    std::string operator()(const lambda& fn) const {
        return "(lambda-function)";
    }

    std::string operator()(const sexprs& v) const {
        std::stringstream ss;
        ss << "(";
        bool first = true;
        for (auto i = v.begin(); i != v.end(); ++i) {
            if (first)
                first = false;
            else
                ss << " ";
            ss << boost::apply_visitor(sexpr2s(), *i);
        }
        ss << ")";
        return ss.str();
    }
};

std::string to_str(const sexpr& l) {
    return boost::apply_visitor(sexpr2s(), l);
}

std::string to_str(const atom& a) {
    return boost::apply_visitor(atom2s(), a);
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
};

sexpr parse_atom(const char* b, const char* e) {
    if (((*b == '-' || *b == '.') && isdigit(*(b+1))) ||
        isdigit(*b)) {
        return atom(boost::lexical_cast<double>(std::string(b, e)));
    }
    else if (*b == '"') {
        return atom(std::string(b+1, e-1));
    }
    else {
        return atom(symbol(b, e));
    }
}

sexpr readref(streamptr& s) {
    if (s.eof()) {
        throw std::runtime_error("unexpected EOF");
    }
    else if (s.peek() == '(') {
        sexprs v;
        s.next();
        while (s.peek() != ')')
            v.push_back(readref(s));
        s.next();
        return v;
    }
    else if (s.peek() == ')') {
        throw std::runtime_error("mismatched )");
    }
    else if (s.peek() == '\'') {
        s.next();
        return list(atom(symbol("quote")), readref(s));
    }
    else {
        const char* b = s._s;
        const char* e = s._s+1;
        while (!isspace(*e) && *e != ')' && *e != '\0')
            ++e;
        s._s = e;
        return parse_atom(b, e);
    }
}

sexpr read(const char* s) {
    streamptr sp(s);
    return readref(sp);
}

class environment {
public:
    environment() {
    }

    environment(const sexprs& vars, const sexprs& args, const boost::shared_ptr<environment> parent) {
        _parent = parent;
        if (vars.size() != args.size())
            throw std::runtime_error("argument arity mismatch");
        for (auto i = vars.begin(), k = args.begin(); i != vars.end(); ++i, ++k) {
            _env[to_str(*i)] = *k;
        }
    }

    environment& add(const char* id, const sexpr& x) {
        _env[id] = x;
        return *this;
    }

    environment& find(const std::string& x) {
        if (_env.find(x) != _env.end())
            return *this;
        else if (_parent)
            return _parent->find(x);
        else
            throw std::runtime_error("Unknown symbol: " + x);
    }

    environment& find(const sexpr& x) {
        return find(to_str(x));
    }

    sexpr& operator[](const std::string& s) {
        return _env[s];
    }

    sexpr& operator[](const sexpr& x) {
        return _env[to_str(x)];
    }

    boost::shared_ptr<environment> _parent;
    std::map<std::string, sexpr> _env;
};

typedef boost::shared_ptr<environment> envptr;

bool is_call_to(const sexprs& v, const char* s) {
    auto a = boost::get<atom>(&v.front());
    if (a) {
        auto sym = boost::get<symbol>(a);
        if (sym) {
            return *sym == s;
        }
    }
    return false;
}

bool truth(const sexpr& x) {
    const sexprs* v = boost::get<sexprs>(&x);
    return !(v && v->size() == 0);
}

sexpr eval(const sexpr& x, const envptr& env);

struct lambda_impl {
    lambda_impl(const sexprs& vars, const sexpr& exp, const envptr& parent) : _vars(vars), _exp(exp), _parent(parent) {
    }
    sexpr operator()(const boost::any& arghack) {
        const sexprs& args = boost::any_cast<const sexprs&>(arghack);
        envptr env(new environment(_vars, args, _parent));
        return eval(_exp, env);
    }
    sexprs _vars;
    sexpr _exp;
    envptr _parent;
};

sexpr make_lambda(const sexprs& vars, const sexpr& exp, const envptr& env) {
    return lambda(lambda_impl(vars, exp, env));
}

sexpr eval(const sexpr& x, const envptr& env) {
    if (auto a = boost::get<atom>(&x)) {
        if (auto s = boost::get<symbol>(a)) {
            return env->find(*s)[*s];
        }
    }
    else if (auto v = boost::get<sexprs>(&x)) {
        if (v->size() == 0) {
            return x;
        }
        else if (is_call_to(*v, "quote")) {
            if (v->size() != 2)
                throw std::runtime_error("incorrect arguments to quote");
            return (*v)[1];
        }
        else if (is_call_to(*v, "if")) {
            if (v->size() == 3) {
                auto& test = (*v)[1];
                auto& conseq = (*v)[2];
                if (truth(eval(test, env))) {
                    return eval(conseq, env);
                }
                else {
                    return sexpr(sexprs());
                }
            }
            else if (v->size() == 4) {
                auto& test = (*v)[1];
                auto& conseq = (*v)[2];
                auto& alt = (*v)[3];
                if (truth(eval(test, env))) {
                    return eval(conseq, env);
                }
                else {
                    return eval(alt, env);
                }
            }
            else {
                throw std::runtime_error("incorrect arguments to if");
            }
        }
        else if (is_call_to(*v, "set!")) {
            if (v->size() != 3)
                throw std::runtime_error("incorrect arguments to set!");
            auto& var = (*v)[1];
            auto& exp = (*v)[2];
            env->find(var)[var] = eval(exp, env);
        }
        else if (is_call_to(*v, "def")) {
            if (v->size() != 3)
                throw std::runtime_error("incorrect arguments to def");
            auto& var = (*v)[1];
            auto& exp = (*v)[2];
            (*env)[var] = eval(exp, env);
        }
        else if (is_call_to(*v, "fn")) {
            if (v->size() != 3)
                throw std::runtime_error("incorrect arguments to fn");
            auto& vars = boost::get<sexprs>((*v)[1]);
            auto& exp = (*v)[2];
            return make_lambda(vars, exp, env);
        }
        else if (is_call_to(*v, "do")) {
            for (size_t i = 1; i < v->size()-1; ++i) {
                eval((*v)[i] , env);
            }
            if (v->size() > 1)
                return eval(v->back(), env);
            return sexprs();
        }
        else {
            lambda proc = boost::get<lambda>(eval((*v)[0], env));
            sexprs exps;
            for (size_t i = 1; i < v->size(); ++i) {
                exps.push_back(eval((*v)[i], env));
            }
            return proc(boost::any(exps));
        }
    }
    return x;
}

namespace {
    using namespace boost;

    sexpr add(const any& arghack) {
        const sexprs& args = any_cast<const sexprs&>(arghack);
        double sum = 0.0;
        for (auto i = args.begin(); i != args.end(); ++i)
            sum += get<double>(get<atom>(*i));
        return atom(sum);
    }

sexpr sub(const any& arghack) {
    const sexprs& args = any_cast<const sexprs&>(arghack);
    if (args.size() == 1) {
        return atom(-get<double>(get<atom>(args.front())));
    }
    else {
        auto i = args.begin();
        double sum = get<double>(get<atom>(*i++));
        for (; i != args.end(); ++i)
            sum -= get<double>(get<atom>(*i));
        return atom(sum);
    }
}
}

int main(int argc, char* argv[]) {
    const int SZ = 1024;
    char tmp[SZ];
    envptr global_env(new environment);

    global_env->add("true", atom(symbol("true")))
        .add("false", sexprs())
        .add("+", lambda(add))
        .add("-", lambda(sub));
    while (true) {
        std::cout << ">>> " << std::flush;
        std::cin.getline(tmp, SZ);
        if (std::cin.eof())
            break;
        if (strcmp(tmp, "quit") == 0)
            break;
        try {
            sexpr exp = eval(read(tmp), global_env);
            global_env->add("_", exp);
            std::cout << "_: " << to_str(exp) << std::endl;
        }
        catch (std::exception& e) {
            std::cout << "Error: " << e.what() << std::endl;
        }
    }
}
