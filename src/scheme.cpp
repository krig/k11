// Fun with variadic templates and boost::variant
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

typedef boost::variant<int, double, std::string, symbol> atom;
typedef boost::make_recursive_variant<atom,
                                      std::vector<boost::recursive_variant_>,
                                      boost::function1<boost::recursive_variant_, const boost::any&>>::type sexpr;
typedef std::vector<sexpr> sexprs;
typedef boost::function1<sexpr, const boost::any&> lambda;

void pushbacker(sexprs& v, int a) {
    v.push_back(atom(a));
}

void pushbacker(sexprs& v, double a) {
    v.push_back(atom(a));
}

void pushbacker(sexprs& v, const symbol& a) {
    v.push_back(atom(a));
}

void pushbacker(sexprs& v, const std::string& a) {
    v.push_back(atom(a));
}

void pushbacker(sexprs& v, const atom& a) {
    v.push_back(a);
}

void pushbacker(sexprs& v, const sexpr& l) {
    v.push_back(l);
}

template <class T, typename... Ts>
void pushbacker(sexprs& v, const T& t, const Ts&... ts) {
    v.push_back(t);
    pushbacker(v, ts...);
}

template <typename... Ts>
sexpr list(const Ts&... ts) {
    sexprs v;
    v.reserve(sizeof...(ts));
    pushbacker(v, ts...);
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

    std::string operator()(int value) const {
        return boost::lexical_cast<std::string>(value);
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
        while (!isspace(*e) && *e != ')' && *e != '\0') {
            if (*e == '.')
                flt = true;
            ++e;
        }
        _s = e;
        std::string sn(n, e);
        if (flt) {
            return boost::lexical_cast<double>(sn);
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
    else if (s.peek() == '"') {
        return s.read_string();
    }
    else if (isdigit(s.peek()) || s.peek() == '.' || s.peek() == '-') {
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

sexpr add(const boost::any& arghack) {
    const sexprs& args = boost::any_cast<const sexprs&>(arghack);
    double dsum = 0.0;
    int isum = 0;
    bool flt = false;
    for (auto i = args.begin(); i != args.end(); ++i) {
        const atom& a = boost::get<atom>(*i);
        if (const int* ival = boost::get<int>(&a)) {
            isum += *ival;
        }
        else if (const double* fval = boost::get<double>(&a)) {
            dsum += *fval;
            flt = true;
        }
        else {
            throw std::runtime_error("Invalid argument to add");
        }
    }
    if (flt) {
        return atom(dsum + (double)isum);
    }
    else {
        return atom(isum);
    }
}

int main(int argc, char* argv[]) {
    const int SZ = 1024;
    char tmp[SZ];
    envptr global_env(new environment);

    global_env->add("true", atom(symbol("true")))
        .add("false", sexprs())
        .add("+", lambda(add));
    while (true) {
        std::cout << ">>> " << std::flush;
        std::cin.getline(tmp, SZ);
        if (std::cin.eof())
            break;
        if (strcmp(tmp, "quit") == 0)
            break;
        try {
            std::cout << to_str(eval(read(tmp), global_env)) << std::endl;
        }
        catch (std::runtime_error& e) {
            std::cout << e.what() << std::endl;
        }
    }
}
