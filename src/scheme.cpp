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
#include <boost/exception/diagnostic_information.hpp>
#include <boost/bind.hpp>
#include <vector>
#include <string>
#include <sstream>
#include <stdexcept>
#include <iostream>
#include <fstream>
#include <stdlib.h>
#include <math.h>
#include "join.hpp"
#include "tokens.hpp"

using namespace std;
using namespace boost;

struct symbol : public string {
    symbol() : string() {}
    symbol(const char* a, const char* b) : string(a, b) {}
    explicit symbol(const char* s) : string(s) {}
    explicit symbol(const string& o) : string(o) {}
};

class procedure;

typedef variant<double, string, symbol> atom;
typedef shared_ptr<procedure> procedure_ptr;
typedef make_recursive_variant<atom,
                               vector<recursive_variant_>,
                               procedure_ptr,
                               function1<recursive_variant_, const void*>>::type sexpr;
typedef vector<sexpr> sexprs;
typedef function1<sexpr, const void*> builtin;

void vec_arg(sexprs& v, int a) { v.push_back(atom((double)a)); }
void vec_arg(sexprs& v, double a) { v.push_back(atom(a)); }
void vec_arg(sexprs& v, const symbol& a) { v.push_back(atom(a)); }
void vec_arg(sexprs& v, const std::string& a) { v.push_back(atom(a)); }
void vec_arg(sexprs& v, const sexpr& l) { v.push_back(l); }
void vec_arg(sexprs& v, const sexprs& l) { v.push_back(l); }

template <class T, typename... Ts>
void vec_arg(sexprs& v, const T& t, const Ts&... ts) {
    vec_arg(v, t);
    vec_arg(v, ts...);
}

template <typename... Ts>
sexprs make_list(const Ts&... ts) {
    sexprs v;
    v.reserve(sizeof...(ts));
    vec_arg(v, ts...);
    return v;
}

string escape_string(const string& s) {
    stringstream ss;
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

struct atom2s : public static_visitor<string> {
    string operator()(const string& x) const { return escape_string(x); }
    string operator()(const symbol& x) const { return x; }
    string operator()(double x) const { return lexical_cast<string>(x); }
};

struct sexpr2s : public static_visitor<string> {
    string operator()(const atom& a) const { return apply_visitor(atom2s(), a); }
    string operator()(const builtin& fn) const { return "<builtin>"; }
    string operator()(const procedure_ptr& fn) const { return "<fn>"; }
    string operator()(const sexprs& v) const {
        stringstream ss;
        ss << "("
           << util::mapjoin(" ", v.begin(), v.end(), [](const sexpr& x) {
                return apply_visitor(sexpr2s(), x);
            })
           << ")";
        return ss.str();
    }
};

string to_str(const sexpr& l) { return apply_visitor(sexpr2s(), l); }

string to_str(const atom& a) { return apply_visitor(atom2s(), a); }

struct syntax_error : public runtime_error {
    explicit syntax_error(const sexpr& sx)
        : runtime_error(to_str(sx) + ": syntax error") {
    }
    syntax_error(const sexpr& sx, const string& msg)
        : runtime_error(to_str(sx) + ": " + msg) {
    }
};

#define REQUIRE(sx, cond) \
    do{if(!(cond)){throw syntax_error(sx, "fails " #cond);}}while(false)

#define REQUIRE2(sx, cond, msg) \
    do{if(!(cond)){throw syntax_error(sx, msg);}}while(false)

sexpr read(token_stream& s);

sexpr read_ahead(token_stream& s, token_stream::Token token) {
    switch (token) {
    case token_stream::Eof:
        return sexprs();
    case token_stream::LParen: {
        sexprs v;
        while (true) {
            token = s.next();
            if (token == token_stream::RParen)
                return v;
            v.push_back(read_ahead(s, token));
        }
    }
    case token_stream::RParen:
        throw runtime_error("unexpected )");
    case token_stream::Quote:
    case token_stream::Quasiquote:
    case token_stream::Unquote:
    case token_stream::UnquoteSplicing: {
        sexprs v;
        v.push_back(atom(symbol(s.text)));
        v.push_back(read(s));
        return v;
    }
    case token_stream::Symbol:
        return atom(symbol(s.text));
    case token_stream::Number:
        return atom(lexical_cast<double>(s.text));
    default:
    case token_stream::String:
        return atom(s.text);
    }
}

struct environment {
    environment()
        : _parent(),
          _env() {
        _env["t"] = atom(symbol("t"));
        _env["f"] = sexprs();
        _env["nil"] = sexprs();
    }

    environment(const sexprs& vars, const sexprs& args, const shared_ptr<environment> parent)
        : _parent(parent),
          _env() {
        _parent = parent;
        if (vars.size() != args.size())
            throw runtime_error("argument arity mismatch");
        for (auto i = vars.begin(), k = args.begin(); i != vars.end(); ++i, ++k) {
            _env[to_str(*i)] = *k;
        }
    }

    environment& add(const char* id, const sexpr& x) {
        _env[id] = x;
        return *this;
    }

    environment& find(const string& x) {
        if (_env.find(x) != _env.end())
            return *this;
        else if (_parent)
            return _parent->find(x);
        else
            throw runtime_error("unknown symbol: " + x);
    }

    environment& find(const sexpr& x) {
        return find(to_str(x));
    }

    sexpr& operator[](const string& s) {
        return _env[s];
    }

    sexpr& operator[](const sexpr& x) {
        return _env[to_str(x)];
    }

    shared_ptr<environment> _parent;
    unordered_map<string, sexpr> _env;
};

typedef shared_ptr<environment> envptr;

namespace {

    bool is_call_to(const sexprs& v, const char* s) {
        auto a = get<atom>(&v.front());
        if (a) {
            auto sym = get<symbol>(a);
            if (sym) {
                return *sym == s;
            }
        }
        return false;
    }

    bool truth(const sexpr& x) {
        const sexprs* v = get<sexprs>(&x);
        return !(v && v->size() == 0);
    }

    envptr global_env;

    map<symbol, sexpr> macro_table;
}

sexpr read(token_stream& s);
sexpr parse(token_stream& s);
sexpr eval(sexpr x, envptr env = global_env);
sexpr expand(sexpr x, bool toplevel = false);
sexpr expand_quasiquote(const sexpr& x);

sexpr read(token_stream& s) {
    return read_ahead(s, s.next());
}

sexpr parse(token_stream& s) {
    return expand(read(s), true);
}

struct procedure {
    procedure(const sexprs& vars, const sexpr& exp, const envptr& parent)
        : _vars(vars), _exp(exp), _parent(parent), _variadic(false) {
        if (vars.size() > 1) {
            const symbol* sym;
            if ((sym = get<symbol>(&get<atom>(vars.back()))) && (*sym == "...")) {
                _variadic = true;
                _vars.pop_back();
            }
        }
    }
    void variadic(sexprs& exps) const {
        if (_variadic) {
            const size_t nargs = _vars.size()-1;
            if (exps.size() < nargs)
                throw runtime_error("bad arity");
            sexprs tail(exps.begin()+nargs, exps.end());
            exps.erase(exps.begin()+nargs, exps.end());
            exps.push_back(tail);
        }
    }
    sexprs _vars;
    sexpr _exp;
    envptr _parent;
    bool _variadic;
};

template <int, typename Fn>
struct builtin_impl;

template <typename Fn>
struct builtin_impl<0, Fn> {
    builtin_impl(const Fn& fn) : _fn(fn) {
    }
    sexpr operator()(const void* arghack) {
        const sexprs& args = *(const sexprs*)(arghack);
        if (args.size() != 0) throw runtime_error("bad arity");
        return _fn();
    }
    function<Fn> _fn;
};

template <typename Fn>
struct builtin_impl<1, Fn> {
    builtin_impl(const Fn& fn) : _fn(fn) {
    }
    sexpr operator()(const void* arghack) {
        const sexprs& args = *(const sexprs*)(arghack);
        if (args.size() != 1) throw runtime_error("bad arity");
        return _fn(args[0]);
    }
    function<Fn> _fn;
};

template <typename Fn>
struct builtin_impl<2, Fn> {
    builtin_impl(const Fn& fn) : _fn(fn) {
    }
    sexpr operator()(const void* arghack) {
        const sexprs& args = *(const sexprs*)(arghack);
        if (args.size() != 2) throw runtime_error("bad arity");
        return _fn(args[0], args[1]);
    }
    function<Fn> _fn;
};

sexpr make_procedure(const sexprs& vars, const sexpr& exp, const envptr& env) {
    return procedure_ptr(new procedure(vars, exp, env));
}

template <typename Fn>
sexpr make_builtin(const Fn& fn) {
    return builtin(builtin_impl<function_traits<Fn>::arity, Fn>(fn));
}

template <typename Fn>
sexpr make_builtin_va(const Fn& fn) {
    return builtin([=](const void* a) {
            return fn(*(const sexprs*)a);
        });
}

sexprs map_expand(sexprs lst, bool toplevel = false) {
    for (sexpr& x : lst)
        x = expand(x, toplevel);
    return lst;
}


sexprs cdr(const sexprs& x) {
    if (x.size() <= 1)
        return sexprs();
    return sexprs(++x.begin(), x.end());
}

sexpr expand(sexpr x, bool toplevel) {
    // macro expansion, todo

    if (get<sexprs>(&x) == nullptr)
        return x; // non-lists pass through
    sexprs& xl = get<sexprs>(x);
    if (xl.size() == 0)
        return xl;

    if (is_call_to(xl, "quote")) {
        REQUIRE(x, xl.size() == 2);
        return x;
    }
    else if (is_call_to(xl, "if")) {
        if (xl.size() == 3)
            xl.push_back(sexprs());
        REQUIRE(x, xl.size() == 4);
        return map_expand(xl);
    }
    else if (is_call_to(xl, "=") || is_call_to(xl, ":")) {
        REQUIRE(x, xl.size() == 3);
        auto var = &xl[1];
        REQUIRE(x, get<symbol>(get<atom>(var)));
        return make_list(xl[0], xl[1], expand(xl[2]));
    }
    else if (is_call_to(xl, "def")) {
        // valid forms: (def foo () body)
        // (def foo (x ...) body)
        REQUIRE(x, xl.size() >= 4);
        auto& _def = xl[0];
        auto& f = xl[1];
        auto& v = xl[2];
        sexprs fn = make_list(symbol("fn"), v);
        fn.insert(fn.end(), xl.begin()+3, xl.end());
        return expand(make_list(symbol(":"), f, fn));
    }
    else if (is_call_to(xl, "defmacro")) {
        // (defmacro foo proc)
        REQUIRE2(x, toplevel, "defmacro only allowed at top level");
        REQUIRE(x, xl.size() == 3);
        auto var = get<symbol>(get<atom>(xl[1]));
        sexpr proc = eval(expand(xl[2]));
        REQUIRE(x, get<procedure_ptr>(proc) || get<builtin>(proc));

        macro_table[var] = proc;

        return sexprs();
    }
    else if (is_call_to(xl, "do")) {
        if (xl.size() == 1)
            return sexprs();
        return map_expand(xl, toplevel);
    }
    else if (is_call_to(xl, "fn")) {
        // (fn (x) e1 e2) => (fn (x) (do e1 e2))
        REQUIRE(x, xl.size() >= 3);
        auto vars = get<sexprs>(&xl[1]);
        REQUIRE(x, vars);
        for (auto& var : *vars)
            REQUIRE(x, get<symbol>(get<atom>(&var)));
        if (xl.size() == 3)
            return make_list(xl[0], *vars, expand(xl[2]));

        sexprs body = make_list(symbol("do"));
        body.insert(body.end(), xl.begin()+2, xl.end());
        return make_list(xl[0], *vars, expand(body));
    }
    else if (is_call_to(xl, "quasiquote")) {
        REQUIRE(x, xl.size() == 2);
        return expand_quasiquote(xl[1]);
    }
    else if (symbol* s = get<symbol>(get<atom>(&xl[0]))) {
        auto mac = macro_table.find(*s);
        if (mac != macro_table.end()) {

            sexprs exps(xl.begin()+1, xl.end());

            if (auto p = get<procedure_ptr>(&(mac->second))) {
                const auto& proc = *(*p);
                proc.variadic(exps);
                envptr env(new environment(proc._vars, exps, proc._parent));
                return expand(eval(proc._exp, env), toplevel);
            }
            else if (auto l = get<builtin>(&(mac->second))) {
                return expand((*l)(&exps), toplevel);
            }
            else {
                throw std::runtime_error("bad data in macro_table");
            }
        }
        else {
            return map_expand(xl);
        }
    }
    else {
        return map_expand(xl);
    }
}

bool is_pair(const sexpr& x) {
    auto p = get<sexprs>(&x);
    return p && p->size() > 0;
}

sexpr expand_quasiquote(const sexpr& x) {
    if (!is_pair(x))
        return make_list(symbol("quote"), x);
    auto& xl = get<sexprs>(x);

    REQUIRE2(x, !is_call_to(xl, "unquote-splicing"), "can't splice here");

    if (is_call_to(xl, "unquote")) {
        REQUIRE(x, xl.size() == 2);
        return xl[1];
    }
    else if (is_pair(xl[0]) && is_call_to(get<sexprs>(xl[0]), "unquote-splicing")) {
        auto& xl0 = get<sexprs>(xl[0]);
        REQUIRE(xl0, xl0.size() == 2);
        return make_list(symbol("append"),
                         xl0[1],
                         expand_quasiquote(cdr(xl)));
    }
    else {
        return make_list(symbol("cons"),
                         expand_quasiquote(xl[0]),
                         expand_quasiquote(cdr(xl)));
    }
}

sexpr eval(sexpr x, envptr env) {
    while (true) {
        if (auto a = get<atom>(&x)) {
            if (auto s = get<symbol>(a)) {
                return env->find(*s)[*s];
            }
            return *a;
        }
        else if (auto v = get<sexprs>(&x)) {
            if (v->size() == 0) {
                return x;
            }
            else if (is_call_to(*v, "quote")) {
                return (*v)[1];
            }
            else if (is_call_to(*v, "if")) {
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
            else if (is_call_to(*v, "=")) {
                auto& var = (*v)[1];
                auto& exp = (*v)[2];
                env->find(var)[var] = eval(exp, env);
                return var;
            }
            else if (is_call_to(*v, ":")) {
                auto& var = (*v)[1];
                auto& exp = (*v)[2];
                (*env)[var] = eval(exp, env);
                return var;
            }
            else if (is_call_to(*v, "def")) {
                auto& var = (*v)[1];
                auto& exp = (*v)[2];
                (*env)[var] = eval(exp, env);
                return var;
            }
            else if (is_call_to(*v, "fn")) {
                auto& vars = get<sexprs>((*v)[1]);
                auto& exp = (*v)[2];
                return make_procedure(vars, exp, env);
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
                sexpr fn = eval((*v)[0], env);
                sexprs exps;
                exps.reserve(vsz);
                for (size_t i = 1; i < vsz; ++i)
                    exps.push_back(eval((*v)[i], env));
                if (auto l = get<builtin>(&fn)) {
                    return (*l)(&exps);
                }
                else if (auto p = get<procedure_ptr>(&fn)) {
                    const auto& proc = *(*p);
                    proc.variadic(exps);
                    x = proc._exp;
                    env = envptr(new environment(proc._vars, exps, proc._parent));
                }
                else
                    throw runtime_error("not callable");
            }
        }
    }
}

struct pratom2s : public static_visitor<string> {
    string operator()(const string& value) const { return value; }
    string operator()(const symbol& value) const { return value; }
    string operator()(double value) const {
        return lexical_cast<string>(value);
    }
};

struct prsexpr2s : public static_visitor<string> {
    string operator()(const atom& a) const {
        return apply_visitor(pratom2s(), a);
    }
    string operator()(const builtin& fn) const { return "<builtin>"; }
    string operator()(const procedure_ptr& fn) const { return "<fn>"; }

    string operator()(const sexprs& v) const {
        if (v.size() == 0)
            return "()";
        stringstream ss;
        ss << "("
           << util::mapjoin(" ", v.begin(), v.end(), [](const sexpr& s) {
                   return apply_visitor(sexpr2s(), s);
               })
           << ")";
        return ss.str();
    }
};

string pr_to_str(const sexpr& l) {
    return apply_visitor(prsexpr2s(), l);
}

void repl(istream& in, bool prompt, bool out);

namespace {
    sexpr addfn(const sexprs& args) {
        double sum = 0.0;
        for (auto v : args)
            sum += get<double>(get<atom>(v));
        return atom(sum);
    }

    sexpr subfn(const sexprs& args) {
        if (args.size() == 1)
            return atom(-get<double>(get<atom>(args.front())));
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
            return sexprs();
        return atom(symbol("t"));
    }

    sexpr listfn(const sexprs& args) {
        return args;
    }

    sexpr lenfn(const sexpr& lst) {
        return atom((double)get<sexprs>(lst).size());
    }

    sexpr carfn(const sexpr& arg) {
        const auto& l = get<sexprs>(arg);
        if (l.size() > 0)
            return l.front();
        return l;
    }

    sexpr cdrfn(const sexpr& arg) {
        const auto& l = get<sexprs>(arg);
        if (l.size() > 0)
            return sexprs(++l.begin(), l.end());
        return l;
    }

    sexpr consfn(const sexprs& args) {
        auto i = args.begin();
        const auto& consing = *i++;
        const auto& lst = get<sexprs>(*i);
        sexprs ret;
        ret.push_back(consing);
        ret.insert(ret.end(), lst.begin(), lst.end());
        return ret;
    }

    sexpr appendfn(const sexprs& args) {
        auto i = args.begin();
        auto lst = get<sexprs>(*i++);
        for (; i != args.end(); ++i) {
            const auto& lst2 = get<sexprs>(*i);
            lst.insert(lst.end(), lst2.begin(), lst2.end());
        }
        return lst;
    }

    sexpr listpfn(const sexpr& arg) {
        auto lst = get<sexprs>(&arg);
        return lst ? *lst : sexprs();
    }
    sexpr nullpfn(const sexpr& arg) {
        auto lst = get<sexprs>(&arg);
        if (lst && lst->size() == 0)
            return atom(symbol("t"));
        return sexprs();
    }
    sexpr symbolpfn(const sexpr& arg) {
        auto a = get<atom>(&arg);
        if (a && get<symbol>(a))
            return *a;
        return sexprs();
    }

    sexpr defvarfn(const sexpr& var, const sexpr& exp) {
        (*global_env)[var] = eval(exp, global_env);
        return var;
    }

    sexpr envfn() {
        for (const auto& e : global_env->_env)
            cout << e.first << "\t=\t" << to_str(e.second) << "\n";
        return sexprs();
    }

    // TODO: generalize to non-numeric types

    sexpr ltfn(const sexpr& a, const sexpr& b) {
        if (get<double>(get<atom>(a)) < get<double>(get<atom>(b)))
            return atom(symbol("t"));
        return sexprs();
    }

    sexpr gtfn(const sexpr& a, const sexpr& b) {
        if (get<double>(get<atom>(a)) > get<double>(get<atom>(b)))
            return atom(symbol("t"));
        return sexprs();
    }

    sexpr lteqfn(const sexpr& a, const sexpr& b) {
        if (get<double>(get<atom>(a)) <= get<double>(get<atom>(b)))
            return atom(symbol("t"));
        return sexprs();
    }

    sexpr gteqfn(const sexpr& a, const sexpr& b) {
        if (get<double>(get<atom>(a)) >= get<double>(get<atom>(b)))
            return atom(symbol("t"));
        return sexprs();
    }

    sexpr eqfn(const sexpr& a0, const sexpr& a1) {
        if (get<atom>(a0) == get<atom>(a1))
            return atom(symbol("t"));
        return sexprs();
    }

    sexpr neqfn(const sexpr& a0, const sexpr& a1) {
        if (get<atom>(a0) == get<atom>(a1))
            return sexprs();
        return atom(symbol("t"));
    }

    sexpr prfn(const sexprs& args) {
        if (args.size() == 0)
            return args;
        for (auto i = args.begin(); i != args.end(); ++i)
            cout << pr_to_str(*i);
        return *args.begin();
    }

    sexpr loadfn(const sexpr& arg) {
        string fname = get<string>(get<atom>(arg));
        ifstream f(fname.c_str());
        if (!f.is_open())
            throw runtime_error("file not found");
        repl(f, false, false);
        return sexprs();
    }

    sexpr cosfn(const sexpr& v) { return atom(cos(get<double>(get<atom>(v)))); }
    sexpr sinfn(const sexpr& v) { return atom(sin(get<double>(get<atom>(v)))); }
    sexpr tanfn(const sexpr& v) { return atom(tan(get<double>(get<atom>(v)))); }
    sexpr acosfn(const sexpr& v) { return atom(acos(get<double>(get<atom>(v)))); }
    sexpr asinfn(const sexpr& v) { return atom(asin(get<double>(get<atom>(v)))); }
    sexpr atanfn(const sexpr& v) { return atom(atan(get<double>(get<atom>(v)))); }

    bool is_list_of_len(const sexpr& x, size_t len) {
        auto l = get<sexprs>(&x);
        return l && l->size() == len;
    }

    bool is_symbol(const sexpr& x) {
        if (auto a = get<atom>(&x))
            return get<symbol>(a) != nullptr;
        return false;
    }

    bool is_even(size_t i) {
        return (i%2) == 0;
    }

    sexpr letfn(const sexprs& args) {
        sexprs x = make_list(symbol("let"));
        x.insert(x.end(), args.begin(), args.end());
        REQUIRE(x, args.size() > 1);
        auto& bindings = args[0];
        auto body = cdr(args);
        REQUIRE2(x, is_pair(bindings), "illegal binding list");
        auto& bindingslist = get<sexprs>(bindings);
        REQUIRE2(x, is_even(bindingslist.size()), "illegal binding list");
        sexprs vars;
        sexprs vals;
        for (size_t i = 0; i < bindingslist.size(); i += 2) {
            REQUIRE2(x, is_symbol(bindingslist[i]), "illegal binding list");
            vars.push_back(bindingslist[i]);
            vals.push_back(bindingslist[i+1]);
        }
        sexprs builtin = make_list(symbol("fn"), vars);
        sexprs builtin_body = map_expand(body);
        builtin.insert(builtin.end(), builtin_body.begin(), builtin_body.end());
        sexprs call;
        call.push_back(builtin);
        sexprs call_body = map_expand(vals);
        call.insert(call.end(), call_body.begin(), call_body.end());
        return call;
    }

    struct call_continuation : public std::exception {
        explicit call_continuation(int depth, const sexpr& exp)
            : std::exception(), _depth(depth), _exp(exp) {
        }
        virtual ~call_continuation() {}
        int _depth;
        sexpr _exp;
    };

    sexpr throwfn(int depth, const sexprs& args) {
        throw call_continuation(depth, args.front());
    }

    sexpr callccfn(const sexprs& args) {
        // arg[0] is procedure
        // call proc with current continuation
        // (escape only)
        static int depth = 0;
        int mydepth = ++depth;
        try {
            auto proc = get<procedure_ptr>(args[0]);
            envptr env(new environment(proc->_vars,
                                       make_list(make_builtin_va(bind(throwfn, depth, _1))),
                                       proc->_parent));
            return eval(proc->_exp, env);
        }
        catch (call_continuation& cc) {
            if (cc._depth < mydepth)
                throw;
            depth = 0;
            return cc._exp;
        }
    }

}



void repl(istream& in, bool prompt, bool out) {
    token_stream tokens(in);
    while (true) {
        if (in.eof())
            break;
        if (prompt)
            cout << ">>> " << flush;
        try {
            sexpr exp = eval(parse(tokens), global_env);
            global_env->add("_", exp);
            if (out)
                cout << to_str(exp) << endl;
        }
        catch (bad_get& e) {
            cerr << "type mismatch: " << diagnostic_information(e) << endl;
        }
        catch (boost::exception& e) {
            cerr << "error: " << diagnostic_information(e) << endl;
        }
        catch (std::exception& e) {
            cerr << "error: " << e.what() << endl;
        }
    }
}

int main(int argc, char* argv[]) {
    macro_table[symbol("let")] = make_builtin_va(letfn);

    global_env = envptr(new environment);

    global_env->
         add("+", make_builtin_va(addfn))
        .add("-", make_builtin_va(subfn))
        .add("*", make_builtin_va(mulfn))
        .add("/", make_builtin_va(divfn))
        .add("not", make_builtin(notfn))
        .add("<", make_builtin(ltfn))
        .add(">", make_builtin(gtfn))
        .add("<=", make_builtin(lteqfn))
        .add(">=", make_builtin(gteqfn))
        .add("==", make_builtin(eqfn))
        .add("!=", make_builtin(neqfn))
        .add("len", make_builtin(lenfn))
        .add("cons", make_builtin_va(consfn))
        .add("car", make_builtin(carfn))
        .add("cdr", make_builtin(cdrfn))
        .add("append", make_builtin_va(appendfn))
        .add("list", make_builtin_va(listfn))
        .add("list?", make_builtin(listpfn))
        .add("null?", make_builtin(nullpfn))
        .add("symbol?", make_builtin(symbolpfn))
        .add("defvar", make_builtin(defvarfn))
        .add("print-globals", make_builtin(envfn))
        .add("pr", make_builtin_va(prfn))
        .add("load", make_builtin(loadfn))
        .add("sin", make_builtin(sinfn))
        .add("cos", make_builtin(cosfn))
        .add("tan", make_builtin(tanfn))
        .add("acos", make_builtin(acosfn))
        .add("asin", make_builtin(asinfn))
        .add("atan", make_builtin(atanfn))
        .add("call/cc", make_builtin_va(callccfn))
        ;
    if (argc > 1) {
        istringstream s(argv[1]);
        repl(s, false, false);
    }
    repl(cin, true, true);
}
