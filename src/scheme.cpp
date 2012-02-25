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
#include "join.hpp"

class token_stream {
public:
    enum Token {
        Eof,
        LParen,
        RParen,
        LMap,
        RMap,
        LVec,
        RVec,
        Symbol,
        Number,
        String,
        Quote,
        Quasiquote,
        Unquote,
        UnquoteSplicing,
    };
    token_stream(std::istream& src) : _src(src) {
    }

    Token next() {
        text = "";

        if (!_src.good())
            return Eof;

        // eat whitespace and comments
        int ch = _src.get();
        while (isspace(ch) || ch == ';') {
            while (isspace(ch))
                ch = _src.get();
            if (ch == ';') {
                while (ch != '\n' && _src.good())
                    ch = _src.get();
            }
        }

        if (!_src.good())
            return Eof;

        switch (ch) {
        case '(': return LParen;
        case ')': return RParen;
        case '{': return LMap;
        case '}': return RMap;
        case '[': return LVec;
        case ']': return RVec;
        case '\'': text = "quote"; return Quote;
        case '`': text = "quasiquote"; return Quasiquote;
        case ',': {
            if (_src.get() == '@') {
                text = "unquote-splicing";
                return UnquoteSplicing;
            }
            _src.unget();
            text = "unquote";
            return Unquote;
        }
        case '"': fill_string(); return String;
        default: {
            fill_text(ch);
            const char* b = text.c_str();
            if (((*b == '-' || *b == '.') && isdigit(*(b+1))) ||
                     isdigit(*b)) {
                return Number;
            }
            else {
                return Symbol;
            }
        }
        }
    }

    std::string text;

private:
    void fill_text(int ch) {
        while (!isspace(ch) && ch != ')') {
            text += (char)ch;
            ch = _src.get();
        }
        _src.unget();
    }

    void fill_string() {
        std::stringstream ss;
        int ch = _src.get();
        while (ch != '"') {
            if (ch == '\\') {
                switch (_src.get()) {
                case '\\': ss << '\\'; break;
                case 'n': ss << '\n'; break;
                case 'r': ss << '\r'; break;
                case 't': ss << '\t'; break;
                case '"': ss << '"'; break;
                default: throw std::runtime_error("unrecognized escape sequence in string");
                }
            }
            else {
                ss << (char)ch;
            }
            ch = _src.get();
        }
        text = ss.str();
    }

    std::istream& _src;
};

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

class procedure;

typedef boost::variant<double, std::string, symbol> atom;
typedef boost::shared_ptr<procedure> procedure_ptr;
typedef boost::make_recursive_variant<atom,
                                      std::vector<boost::recursive_variant_>,
                                      procedure_ptr,
                                      boost::function1<boost::recursive_variant_, const void*>>::type sexpr;
typedef std::vector<sexpr> sexprs;
typedef boost::function1<sexpr, const void*> lambda;

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
        return "<builtin>";
    }

    std::string operator()(const procedure_ptr& fn) const {
        return "<fn>";
    }

    std::string operator()(const sexprs& v) const {
        if (v.size() == 0)
            return "nil";

        std::stringstream ss;
        ss << "("
           << util::mapjoin(" ", v.begin(), v.end(), [](const sexpr& x) {
                return boost::apply_visitor(sexpr2s(), x);
            })
           << ")";
        return ss.str();
    }
};

std::string to_str(const sexpr& l) {
    return boost::apply_visitor(sexpr2s(), l);
}

std::string to_str(const atom& a) {
    return boost::apply_visitor(atom2s(), a);
}


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
        throw std::runtime_error("unexpected )");
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
        return atom(boost::lexical_cast<double>(s.text));
    default:
    case token_stream::String:
        return atom(s.text);
    }
}

sexpr expand(const sexpr& x, bool toplevel = false) {
    // macro expansion, todo
    return x;
}

sexpr read(token_stream& s) {
    token_stream::Token tok = s.next();
    return expand(read_ahead(s, tok), true);
}

class environment {
public:
    environment()
        : _parent(),
          _env() {
        _env["t"] = atom(symbol("t"));
        _env["f"] = sexprs();
        _env["nil"] = sexprs();
    }

    environment(const sexprs& vars, const sexprs& args, const boost::shared_ptr<environment> parent)
        : _parent(parent),
          _env() {
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

sexpr eval(sexpr x, envptr env);

class procedure {
public:
    procedure(const sexprs& vars, const sexpr& exp, const envptr& parent)
        : _vars(vars), _exp(exp), _parent(parent), _variadic(false) {
        const symbol* sym;
        if ((sym = boost::get<symbol>(&boost::get<atom>(vars.back()))) && (*sym == "...")) {
            _variadic = true;
            _vars.pop_back();
        }
    }
    sexprs _vars;
    sexpr _exp;
    envptr _parent;
    bool _variadic;
};

template <int, typename Fn>
struct lambda_impl_2;

template <typename Fn>
struct lambda_impl_2<0, Fn> {
    lambda_impl_2(const Fn& fn) : _fn(fn) {
    }
    sexpr operator()(const void* arghack) {
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
    sexpr operator()(const void* arghack) {
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
    sexpr operator()(const void* arghack) {
        const sexprs& args = *(const sexprs*)(arghack);
        if (args.size() != 2) throw std::runtime_error("bad arity");
        return _fn(args[0], args[1]);
    }
    boost::function<Fn> _fn;
};

sexpr make_procedure(const sexprs& vars, const sexpr& exp, const envptr& env) {
    return procedure_ptr(new procedure(vars, exp, env));
}

template <typename Fn>
sexpr make_lambda(const Fn& fn) {
    return lambda(lambda_impl_2<boost::function_traits<Fn>::arity, Fn>(fn));
}

template <typename Fn>
sexpr make_lambda_va(const Fn& fn) {
    return lambda([=](const void* a) {
            return fn(*(const sexprs*)a);
        });
}

sexpr eval(sexpr x, envptr env) {
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
                if (auto l = boost::get<lambda>(&fn)) {
                    return (*l)(&exps);
                }
                else if (auto p = boost::get<procedure_ptr>(&fn)) {
                    const auto& proc = *(*p);
                    if (proc._variadic) {
                        const size_t nargs = proc._vars.size()-1;
                        if (exps.size() < nargs)
                            throw std::runtime_error("bad arity");
                        sexprs tail(exps.begin()+nargs, exps.end());
                        exps.erase(exps.begin()+nargs, exps.end());
                        exps.push_back(tail);
                    }
                    x = proc._exp;
                    env = envptr(new environment(proc._vars, exps, proc._parent));
                }
                else
                    throw std::runtime_error("not callable");
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
        return "<builtin>";
    }

    std::string operator()(const procedure_ptr& fn) const {
        return "<fn>";
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
        for (auto i : args)
            sum += get<double>(get<atom>(i));
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
            return atom(symbol("t"));
        }
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
            std::cout << e.first << "\t=\t" << to_str(e.second) << "\n";
        return sexprs();
    }

    // TODO: generalize for non-numeric types

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
            std::cout << pr_to_str(*i);
        return *args.begin();
    }

    sexpr loadfn(const sexpr& arg) {
        std::string fname = get<std::string>(get<atom>(arg));

        std::ifstream f(fname.c_str());
        if (!f.is_open())
            throw std::runtime_error("file not found");

        repl(f, false, false);
        return sexprs();
    }

    sexpr cosfn(const sexpr& v) { return atom(cos(get<double>(get<atom>(v)))); }
    sexpr sinfn(const sexpr& v) { return atom(sin(get<double>(get<atom>(v)))); }
    sexpr tanfn(const sexpr& v) { return atom(tan(get<double>(get<atom>(v)))); }
    sexpr acosfn(const sexpr& v) { return atom(acos(get<double>(get<atom>(v)))); }
    sexpr asinfn(const sexpr& v) { return atom(asin(get<double>(get<atom>(v)))); }
    sexpr atanfn(const sexpr& v) { return atom(atan(get<double>(get<atom>(v)))); }

}



void repl(std::istream& in, bool prompt, bool out) {
    token_stream tokens(in);
    while (true) {
        if (in.eof())
            break;
        if (prompt)
            std::cout << ">>> " << std::flush;
        try {
            sexpr exp = eval(read(tokens), global_env);
            global_env->add("_", exp);
            if (out)
                std::cout << "  " << to_str(exp) << std::endl;
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
        .add("pr", make_lambda_va(prfn))
        .add("load", make_lambda(loadfn))
        .add("sin", make_lambda(sinfn))
        .add("cos", make_lambda(cosfn))
        .add("tan", make_lambda(tanfn))
        .add("acos", make_lambda(acosfn))
        .add("asin", make_lambda(asinfn))
        .add("atan", make_lambda(atanfn))
        ;
    if (argc > 1) {
        std::istringstream s(argv[1]);
        repl(s, false, false);
    }
    repl(std::cin, true, true);
}
