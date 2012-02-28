#include "tokens.hpp"
#include <sstream>

token_stream::token_stream(std::istream& src) : _src(src) {
}

token_stream::Token token_stream::next() {
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

void token_stream::fill_text(int ch) {
    while (!isspace(ch) && ch != ')' &&
           ch != '}' &&
           ch != ']') {
        text += (char)ch;
        ch = _src.get();
    }
    _src.unget();
}

void token_stream::fill_string() {
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
            default: throw token_error("unrecognized escape sequence in string");
            }
        }
        else {
            ss << (char)ch;
        }
        ch = _src.get();
    }
    text = ss.str();
}
