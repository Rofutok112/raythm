#pragma once

#include <string>
#include <vector>

namespace mv {

enum class token_type {
    // Literals
    number, string_lit, true_lit, false_lit, none_lit,

    // Identifiers & keywords
    ident,
    kw_def, kw_return, kw_if, kw_elif, kw_else,
    kw_for, kw_in, kw_and, kw_or, kw_not,

    // Operators
    plus, minus, star, slash, percent, power,    // + - * / % **
    eq, ne, lt, gt, le, ge,                       // == != < > <= >=
    assign,                                        // =
    plus_assign, minus_assign, star_assign,        // += -= *=
    slash_assign, percent_assign,                  // /= %=

    // Delimiters
    lparen, rparen,         // ( )
    lbracket, rbracket,     // [ ]
    comma, colon, dot,      // , : .

    // Structure
    newline, indent, dedent,

    // Special
    eof, error,
};

struct token {
    token_type type = token_type::eof;
    std::string text;
    int line = 0;
    int column = 0;
};

struct lex_result {
    std::vector<token> tokens;
    std::vector<std::string> errors;
    bool success = true;
};

lex_result lex(const std::string& source);

} // namespace mv
