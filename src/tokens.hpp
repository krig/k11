#pragma once

#include <istream>
#include <string>
#include <stdexcept>
#include <stdlib.h>

struct token_error : public std::runtime_error {
    token_error(const char* msg) : std::runtime_error(msg) {}
};

struct token_stream {
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
    token_stream(std::istream& src);

    Token next();

    std::string text;

private:
    void fill_text(int ch);
    void fill_string();
    std::istream& _src;
};
