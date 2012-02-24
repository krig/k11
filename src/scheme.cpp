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
#include <boost/type_traits.hpp>
#include <boost/unordered_map.hpp>
#include <vector>
#include <string>
#include <sstream>
#include <stdexcept>
#include <iostream>
#include <fstream>
#include <stdlib.h>
#include <math.h>

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

class context;

typedef boost::variant<double, std::string, symbol> atom;
typedef boost::make_recursive_variant<atom,
                                      std::vector<boost::recursive_variant_>,
                                      boost::function2<boost::recursive_variant_, const void*, context*>>::type sexpr;
typedef std::vector<sexpr> sexprs;
typedef boost::function2<sexpr, const void*, context*> lambda;

template <typename Atom>
void vec_arg(sexprs& v, const Atom& a) {
    v.push_back(atom(a));
}

void vec_arg(sexprs& v, int a) {
    v.push_back(atom((double)a));
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

std::string escape_string(const std::string& s) {
    std::stringstream ss;
    ss << '"';
    for (auto i = s.begin(); i != s.end(); ++i) {
        if (isgraph(*i) || *i == ' ')
            ss << *i;
        else {
            switch (*i) {
            case '\\': ss << "\\\\"; break;
            case '\n': ss << "\\n"; break;
            case '\t': ss << "\\t"; break;
            case '"': ss << "\\\""; break;
            default: ss << "\\?"; break;
            }
        }
    }
    ss << '"';
    return ss.str();
}

class atom2s : public boost::static_visitor<std::string>
{
public:
    std::string operator()(const std::string& value) const {
        return escape_string(value);
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
        return "<fn>";
    }

    std::string operator()(const sexprs& v) const {
        if (v.size() == 0)
            return "nil";

        std::stringstream ss;
        ss << "(";
        auto i = v.begin();
        if (i != v.end()) {
            ss << boost::apply_visitor(sexpr2s(), *i++);
        }
        for (; i != v.end(); ++i) {
            ss << " " << boost::apply_visitor(sexpr2s(), *i);
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
        return *_s == 0 || *_s == ';';
    }
    char next() {
        while (isspace(*_s))
            ++_s;
        return *_s++;
    }
};

sexpr unescape_string(const char* s, const char* e) {
    std::stringstream ss;
    while (s != e) {
        if (*s == '\\') {
            switch (*(++s)) {
            case '\\': ss << '\\'; break;
            case 'n': ss << '\n'; break;
            case 'r': ss << '\r'; break;
            case 't': ss << '\t'; break;
            case '"': ss << '"'; break;
            default: throw std::runtime_error("unrecognized escape sequence in string");
            }
        }
        else {
            ss << *s;
        }
        ++s;
    }
    return atom(ss.str());
}

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

sexpr read2(streamptr& s) {
    streamptr sp(s);

    // skip comment
    if (s.peek() == ';') {
        while (s.next() != '\n' && !s.eof())
            ;
    }

    if (s.eof()) {
        return sexprs();
    }
    else if (s.peek() == '\n') {
        return sexprs();
    }
    else if (s.peek() == '(') {
        sexprs v;
        s.next();
        while (s.peek() != ')')
            v.push_back(read2(s));
        s.next();
        return v;
    }
    else if (s.peek() == ')') {
        throw std::runtime_error("mismatched )");
    }
    else if (s.peek() == '\'') {
        s.next();
        return list(atom(symbol("quote")), read2(s));
    }
    else if (s.peek() == '`') {
        s.next();
        return list(atom(symbol("quasiquote")), read2(s));
    }
    else if (s.peek() == ',') {
        s.next();
        if (s.peek() == '@')
            return list(atom(symbol("unquote-splicing")), read2(s));
        return list(atom(symbol("unquote")), read2(s));
    }
    else if (s.peek() == '"') {
        const char* b = s._s+1;
        const char* e = b;
        while (!(*(e-1) == '\\' && *e == '"') && (*e != '"'))
            ++e;
        s._s = e+1;
        return unescape_string(b, e);
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
    return read2(sp);
}

class environment {
public:
    environment()
        : _parent(),
          _env(),
          _true(symbol("true")),
          _false() {
        _env["true"] = _true;
        _env["false"] = _false;
        _env["nil"] = _false;
    }

    environment(const sexprs& vars, const sexprs& args, const boost::shared_ptr<environment> parent)
        : _parent(parent),
          _env(),
          _true(symbol("true")),
          _false() {
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
            throw std::runtime_error("unknown symbol: " + x);
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
    boost::unordered_map<std::string, sexpr> _env;

    atom _true;
    sexprs _false;
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

class context {
public:
    explicit context(environment* env_) : env(env_) {}
    environment* env;
};

sexpr eval(sexpr x, const envptr& env);

struct lambda_impl {
    lambda_impl(const sexprs& vars, const sexpr& exp, const envptr& parent) : _vars(vars), _exp(exp), _parent(parent) {
    }
    sexpr operator()(const void* arghack, context*) {
        const sexprs& args = *(const sexprs*)(arghack);
        envptr env(new environment(_vars, args, _parent));
        return eval(_exp, env);
    }
    sexprs _vars;
    sexpr _exp;
    envptr _parent;
};

template <int, typename Fn>
struct lambda_impl_2;

template <typename Fn>
struct lambda_impl_2<0, Fn> {
    lambda_impl_2(const Fn& fn) : _fn(fn) {
    }
    sexpr operator()(const void* arghack, context*) {
        const sexprs& args = *(const sexprs*)(arghack);
        if (args.size() != 0) throw std::runtime_error("bad arity");
        return _fn();
    }
    boost::function<Fn> _fn;
};

template <typename Fn>
struct lambda_impl_2<1, Fn> {
    lambda_impl_2(const Fn& fn) : _fn(fn) {
    }
    sexpr operator()(const void* arghack, context*) {
        const sexprs& args = *(const sexprs*)(arghack);
        if (args.size() != 1) throw std::runtime_error("bad arity");
        return _fn(args[0]);
    }
    boost::function<Fn> _fn;
};

template <typename Fn>
struct lambda_impl_2<2, Fn> {
    lambda_impl_2(const Fn& fn) : _fn(fn) {
    }
    sexpr operator()(const void* arghack, context*) {
        const sexprs& args = *(const sexprs*)(arghack);
        if (args.size() != 2) throw std::runtime_error("bad arity");
        return _fn(args[0], args[1]);
    }
    boost::function<Fn> _fn;
};

template <int, typename Fn>
struct lambda_impl_va;

template <typename Fn>
struct lambda_impl_va<1, Fn> {
    lambda_impl_va(const Fn& fn) : _fn(fn) {
    }
    sexpr operator()(const void* arghack, context*) {
        return _fn(*(const sexprs*)(arghack));
    }
    boost::function<Fn> _fn;
};

template <typename Fn>
struct lambda_impl_va<2, Fn> {
    lambda_impl_va(const Fn& fn) : _fn(fn) {
    }
    sexpr operator()(const void* arghack, context* ctx) {
        return _fn(*(const sexprs*)(arghack), ctx);
    }
    boost::function<Fn> _fn;
};

sexpr make_lambda(const sexprs& vars, const sexpr& exp, const envptr& env) {
    return lambda(lambda_impl(vars, exp, env));
}

template <typename Fn>
sexpr make_lambda(const Fn& fn) {
    return lambda(lambda_impl_2<boost::function_traits<Fn>::arity, Fn>(fn));
}

template <typename Fn>
sexpr make_lambda_va(const Fn& fn) {
    return lambda(lambda_impl_va<boost::function_traits<Fn>::arity, Fn>(fn));
}

sexpr eval(sexpr x, const envptr& env) {
    while (true) {
        if (auto a = boost::get<atom>(&x)) {
            if (auto s = boost::get<symbol>(a)) {
                return env->find(*s)[*s];
            }
            return *a;
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
                        x = conseq;
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
                        x = conseq;
                    }
                    else {
                        x = alt;
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
                return var;
            }
            else if (is_call_to(*v, "def")) {
                if (v->size() != 3)
                    throw std::runtime_error("incorrect arguments to def");
                auto& var = (*v)[1];
                auto& exp = (*v)[2];
                (*env)[var] = eval(exp, env);
                return var;
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
                    x = v->back();
                else
                    return sexprs();
            }
            else {
                const size_t vsz = v->size();
                lambda proc = boost::get<lambda>(eval((*v)[0], env));
                sexprs exps;
                exps.reserve(vsz);
                for (size_t i = 1; i < vsz; ++i)
                    exps.push_back(eval((*v)[i], env));
                context ctx(env.get());
                return proc(&exps, &ctx);
            }
        }
    }
}

class pratom2s : public boost::static_visitor<std::string>
{
public:
    std::string operator()(const std::string& value) const {
        return value;
    }

    std::string operator()(const symbol& value) const {
        return value;
    }

    std::string operator()(double value) const {
        char tmp[80];
        snprintf(tmp, sizeof(tmp), "%g", value);
        return tmp;
    }
};

class prsexpr2s : public boost::static_visitor<std::string>
{
public:

    std::string operator()(const atom& a) const {
        return boost::apply_visitor(pratom2s(), a);
    }

    std::string operator()(const lambda& fn) const {
        return "(lambda-function)";
    }

    std::string operator()(const sexprs& v) const {
        if (v.size() == 0)
            return "nil";

        std::stringstream ss;
        ss << "(";
        auto i = v.begin();
        if (i != v.end()) {
            ss << boost::apply_visitor(prsexpr2s(), *i++);
        }
        for (; i != v.end(); ++i) {
            ss << " " << boost::apply_visitor(prsexpr2s(), *i);
        }
        ss << ")";
        return ss.str();
    }
};

std::string pr_to_str(const sexpr& l) {
    return boost::apply_visitor(prsexpr2s(), l);
}

void repl(std::istream& in, bool prompt, bool out);

namespace {
    using namespace boost;

    envptr global_env;

    sexpr addfn(const sexprs& args) {
        double sum = 0.0;
        for (auto i = args.begin(); i != args.end(); ++i)
            sum += get<double>(get<atom>(*i));
        return atom(sum);
    }

    sexpr subfn(const sexprs& args) {
        if (args.size() == 1) {
            return atom(-get<double>(get<atom>(args.front())));
        }
        auto i = args.begin();
        double sum = get<double>(get<atom>(*i++));
        for (; i != args.end(); ++i)
            sum -= get<double>(get<atom>(*i));
        return atom(sum);
    }

    sexpr mulfn(const sexprs& args) {
        auto i = args.begin();
        double sum = get<double>(get<atom>(*i++));
        for (; i != args.end(); ++i)
            sum *= get<double>(get<atom>(*i));
        return atom(sum);
    }

    sexpr divfn(const sexprs& args) {
        auto i = args.begin();
        double sum = get<double>(get<atom>(*i++));
        for (; i != args.end(); ++i)
            sum /= get<double>(get<atom>(*i));
        return atom(sum);
    }

    sexpr notfn(const sexpr& arg) {
        if (truth(arg))
            return global_env->_false;
        return global_env->_true;
    }

    sexpr listfn(const sexprs& args) {
        return args;
    }

    sexpr lenfn(const sexpr& lst) {
        return atom((double)get<sexprs>(lst).size());
    }

    sexpr carfn(const sexpr& arg) {
        return get<sexprs>(arg).front();
    }

    sexpr cdrfn(const sexpr& arg) {
        const auto& lst = get<sexprs>(arg);
        return sexprs(++lst.begin(), lst.end());
    }

    sexpr consfn(const sexprs& args) {
        auto i = args.begin();
        const auto& consing = *i++;
        const auto& lst = get<sexprs>(*i);
        sexprs ret;
        ret.reserve(lst.size()+1);
        ret.push_back(consing);
        ret.insert(ret.end(), lst.begin(), lst.end());
        return ret;
    }

    sexpr appendfn(const sexprs& args) {
        auto i = args.begin();
        const auto& lst = get<sexprs>(*i++);
        sexprs ret(lst);
        for (; i != args.end(); ++i)
            ret.push_back(*i);
        return ret;
    }

    sexpr listpfn(const sexpr& arg) {
        auto lst = get<sexprs>(&arg);
        return lst ? *lst : sexprs();
    }
    sexpr nullpfn(const sexpr& arg) {
        auto lst = get<sexprs>(&arg);
        if (lst && lst->size() == 0) {
            return global_env->_true;
        }
        return global_env->_false;
    }
    sexpr symbolpfn(const sexpr& arg) {
        auto a = get<atom>(&arg);
        if (a && get<symbol>(a))
            return *a;
        return global_env->_false;
    }

    sexpr defvarfn(const sexpr& var, const sexpr& exp) {
        (*global_env)[var] = eval(exp, global_env);
        return var;
    }

    sexpr envfn() {
        auto i = global_env->_env.begin(), e = global_env->_env.end();
        for (; i != e; ++i) {
            std::cout << i->first << "\t=\t" << to_str(i->second) << "\n";
        }
        return global_env->_false;
    }

    sexpr localsfn(const sexprs&, context* ctx) {
        environment* env = ctx->env;
        for (auto i = env->_env.begin(), e = env->_env.end(); i != e; ++i) {
            std::cout << i->first << "\t=\t" << to_str(i->second) << "\n";
        }
        return global_env->_false;
    }

    // TODO: generalize for non-numeric types

    sexpr ltfn(const sexpr& a, const sexpr& b) {
        if (get<double>(get<atom>(a)) < get<double>(get<atom>(b)))
            return global_env->_true;
        return global_env->_false;
    }

    sexpr gtfn(const sexpr& a, const sexpr& b) {
        if (get<double>(get<atom>(a)) > get<double>(get<atom>(b)))
            return global_env->_true;
        return global_env->_false;
    }

    sexpr lteqfn(const sexpr& a, const sexpr& b) {
        if (get<double>(get<atom>(a)) <= get<double>(get<atom>(b)))
            return global_env->_true;
        return global_env->_false;
    }

    sexpr gteqfn(const sexpr& a, const sexpr& b) {
        if (get<double>(get<atom>(a)) >= get<double>(get<atom>(b)))
            return global_env->_true;
        return global_env->_false;
    }

    sexpr eqfn(const sexpr& a0, const sexpr& a1) {
        if (get<atom>(a0) == get<atom>(a1))
            return global_env->_true;
        return global_env->_false;
    }

    sexpr neqfn(const sexpr& a0, const sexpr& a1) {
        if (get<atom>(a0) == get<atom>(a1))
            return global_env->_false;
        return global_env->_true;
    }

    sexpr prfn(const sexprs& args) {
        if (args.size() == 0)
            return args;
        for (auto i = args.begin(); i != args.end(); ++i)
            std::cout << pr_to_str(*i);
        return *args.begin();
    }

    sexpr loadfn(const sexpr& arg) {
        std::string fname = get<std::string>(get<atom>(arg));

        std::ifstream f(fname.c_str());
        if (!f.is_open())
            throw std::runtime_error("file not found");

        repl(f, false, false);
        return global_env->_false;
    }

    sexpr cosfn(const sexpr& v) { return atom(cos(get<double>(get<atom>(v)))); }
    sexpr sinfn(const sexpr& v) { return atom(sin(get<double>(get<atom>(v)))); }
    sexpr tanfn(const sexpr& v) { return atom(tan(get<double>(get<atom>(v)))); }
    sexpr acosfn(const sexpr& v) { return atom(acos(get<double>(get<atom>(v)))); }
    sexpr asinfn(const sexpr& v) { return atom(asin(get<double>(get<atom>(v)))); }
    sexpr atanfn(const sexpr& v) { return atom(atan(get<double>(get<atom>(v)))); }

}



void repl(std::istream& in, bool prompt, bool out) {
    const int SZ = 1024;
    char tmp[SZ];
    while (true) {
        if (prompt)
            std::cout << ">>> " << std::flush;
        in.getline(tmp, SZ);
        if (in.eof())
            break;
        if (tmp[0] == '\0')
            continue;
        if (strcmp(tmp, "quit") == 0)
            break;
        try {
            sexpr exp = eval(read(tmp), global_env);
            global_env->add("_", exp);
            if (out)
                std::cout << "_: " << to_str(exp) << std::endl;
        }
        catch (boost::bad_get& e) {
            std::cerr << "error: type mismatch" << std::endl;
        }
        catch (std::exception& e) {
            std::cerr << "error: " << e.what() << std::endl;
        }
    }
}

int main(int argc, char* argv[]) {
    global_env = envptr(new environment);

    global_env->
         add("+", make_lambda_va(addfn))
        .add("-", make_lambda_va(subfn))
        .add("*", make_lambda_va(mulfn))
        .add("/", make_lambda_va(divfn))
        .add("not", make_lambda(notfn))
        .add("<", make_lambda(ltfn))
        .add(">", make_lambda(gtfn))
        .add("<=", make_lambda(lteqfn))
        .add(">=", make_lambda(gteqfn))
        .add("==", make_lambda(eqfn))
        .add("!=", make_lambda(neqfn))
        .add("len", make_lambda(lenfn))
        .add("cons", make_lambda_va(consfn))
        .add("car", make_lambda(carfn))
        .add("cdr", make_lambda(cdrfn))
        .add("append", make_lambda_va(appendfn))
        .add("list", make_lambda_va(listfn))
        .add("list?", make_lambda(listpfn))
        .add("null?", make_lambda(nullpfn))
        .add("symbol?", make_lambda(symbolpfn))
        .add("defvar", make_lambda(defvarfn))
        .add("print-globals", make_lambda(envfn))
        .add("print-locals", make_lambda_va(localsfn))
        .add("pr", make_lambda_va(prfn))
        .add("load", make_lambda(loadfn))
        .add("sin", make_lambda(sinfn))
        .add("cos", make_lambda(cosfn))
        .add("tan", make_lambda(tanfn))
        .add("acos", make_lambda(acosfn))
        .add("asin", make_lambda(asinfn))
        .add("atan", make_lambda(atanfn))
        ;
    repl(std::cin, true, true);
}
