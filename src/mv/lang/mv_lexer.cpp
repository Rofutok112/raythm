#include "mv_lexer.h"

#include <algorithm>
#include <cstdlib>
#include <sstream>
#include <string_view>

namespace mv {

namespace {

bool is_alpha(char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_'; }
bool is_digit(char c) { return c >= '0' && c <= '9'; }
bool is_alnum(char c) { return is_alpha(c) || is_digit(c); }

struct lexer_state {
    const std::string& source;
    int pos = 0;
    int line = 1;
    int column = 1;
    std::vector<int> indent_stack = {0};
    std::vector<token> tokens;
    std::vector<std::string> errors;
    bool at_line_start = true;
    bool has_error = false;

    explicit lexer_state(const std::string& source_ref) : source(source_ref) {
        tokens.reserve(std::max<std::size_t>(32, source.size() / 4));
        errors.reserve(8);
    }

    char peek() const { return pos < static_cast<int>(source.size()) ? source[pos] : '\0'; }
    char peek_next() const { return pos + 1 < static_cast<int>(source.size()) ? source[pos + 1] : '\0'; }
    bool at_end() const { return pos >= static_cast<int>(source.size()); }

    char advance() {
        char c = source[pos++];
        if (c == '\n') { line++; column = 1; }
        else { column++; }
        return c;
    }

    void emit(token_type type, const std::string& text, int tok_line, int tok_col) {
        tokens.push_back({type, text, 0.0, false, tok_line, tok_col});
    }

    void emit_owned(token_type type, std::string&& text, int tok_line, int tok_col) {
        tokens.push_back({type, std::move(text), 0.0, false, tok_line, tok_col});
    }

    void emit_number(std::string text, double value, int tok_line, int tok_col) {
        tokens.push_back({token_type::number, std::move(text), value, true, tok_line, tok_col});
    }

    void error(const std::string& msg) {
        std::ostringstream oss;
        oss << "line " << line << ":" << column << ": " << msg;
        errors.push_back(oss.str());
        has_error = true;
    }

    void skip_comment() {
        while (!at_end() && peek() != '\n') advance();
    }

    void process_indent() {
        int spaces = 0;
        while (!at_end() && peek() == ' ') {
            advance();
            spaces++;
        }
        // Tabs: treat each as 4 spaces
        while (!at_end() && peek() == '\t') {
            advance();
            spaces += 4;
        }

        // Skip blank lines and comment-only lines
        if (at_end() || peek() == '\n' || peek() == '#') return;

        int current_indent = indent_stack.back();
        if (spaces > current_indent) {
            indent_stack.push_back(spaces);
            emit(token_type::indent, "", line, 1);
        } else {
            while (spaces < indent_stack.back()) {
                indent_stack.pop_back();
                emit(token_type::dedent, "", line, 1);
            }
            if (spaces != indent_stack.back()) {
                error("inconsistent indentation");
            }
        }
    }

    token_type keyword_or_ident(std::string_view text) {
        if (text == "def") return token_type::kw_def;
        if (text == "return") return token_type::kw_return;
        if (text == "if") return token_type::kw_if;
        if (text == "elif") return token_type::kw_elif;
        if (text == "else") return token_type::kw_else;
        if (text == "for") return token_type::kw_for;
        if (text == "in") return token_type::kw_in;
        if (text == "and") return token_type::kw_and;
        if (text == "or") return token_type::kw_or;
        if (text == "not") return token_type::kw_not;
        if (text == "True") return token_type::true_lit;
        if (text == "False") return token_type::false_lit;
        if (text == "None") return token_type::none_lit;
        return token_type::ident;
    }

    bool is_forbidden_keyword(std::string_view text) {
        return text == "import" || text == "class" || text == "eval" ||
               text == "exec" || text == "global" || text == "nonlocal" ||
               text == "open" || text == "from" || text == "with" ||
               text == "yield" || text == "async" || text == "await" ||
               text == "lambda" || text == "del" || text == "raise" ||
               text == "try" || text == "except" || text == "finally";
    }

    void lex_number() {
        int tok_line = line, tok_col = column;
        int start = pos;
        while (!at_end() && is_digit(peek())) advance();
        if (!at_end() && peek() == '.' && is_digit(peek_next())) {
            advance(); // consume '.'
            while (!at_end() && is_digit(peek())) advance();
        }
        emit_number(source.substr(start, pos - start),
                    std::strtod(source.c_str() + start, nullptr),
                    tok_line,
                    tok_col);
    }

    void lex_string(char quote) {
        int tok_line = line, tok_col = column;
        advance(); // consume opening quote
        std::string value;
        value.reserve(16);
        while (!at_end() && peek() != quote && peek() != '\n') {
            if (peek() == '\\') {
                advance();
                if (at_end()) break;
                char escaped = advance();
                switch (escaped) {
                    case 'n': value += '\n'; break;
                    case 't': value += '\t'; break;
                    case '\\': value += '\\'; break;
                    case '\'': value += '\''; break;
                    case '"': value += '"'; break;
                    default: value += '\\'; value += escaped; break;
                }
            } else {
                value += advance();
            }
        }
        if (at_end() || peek() == '\n') {
            error("unterminated string literal");
            return;
        }
        advance(); // consume closing quote
        emit_owned(token_type::string_lit, std::move(value), tok_line, tok_col);
    }

    void lex_identifier() {
        int tok_line = line, tok_col = column;
        int start = pos;
        while (!at_end() && is_alnum(peek())) advance();
        const std::string_view text(source.data() + start, static_cast<std::size_t>(pos - start));

        if (is_forbidden_keyword(text)) {
            error("'" + std::string(text) + "' is not allowed in MV scripts");
            return;
        }

        token_type type = keyword_or_ident(text);
        if (type == token_type::ident) {
            emit_owned(type, std::string(text), tok_line, tok_col);
        } else {
            emit(type, std::string(text), tok_line, tok_col);
        }
    }

    void lex_all() {
        while (!at_end()) {
            if (has_error && errors.size() > 10) break;

            if (at_line_start) {
                at_line_start = false;
                process_indent();
                if (at_end()) break;
            }

            char c = peek();

            if (c == '\n') {
                // Only emit newline if last token is meaningful
                if (!tokens.empty()) {
                    auto last = tokens.back().type;
                    if (last != token_type::newline && last != token_type::indent &&
                        last != token_type::dedent) {
                        emit(token_type::newline, "", line, column);
                    }
                }
                advance();
                at_line_start = true;
                continue;
            }

            if (c == '#') {
                skip_comment();
                continue;
            }

            if (c == ' ' || c == '\t' || c == '\r') {
                advance();
                continue;
            }

            if (is_digit(c)) { lex_number(); continue; }
            if (c == '"' || c == '\'') { lex_string(c); continue; }
            if (is_alpha(c)) { lex_identifier(); continue; }

            int tok_line = line, tok_col = column;

            // Two-character operators
            if (c == '*' && peek_next() == '*') { advance(); advance(); emit(token_type::power, "**", tok_line, tok_col); continue; }
            if (c == '=' && peek_next() == '=') { advance(); advance(); emit(token_type::eq, "==", tok_line, tok_col); continue; }
            if (c == '!' && peek_next() == '=') { advance(); advance(); emit(token_type::ne, "!=", tok_line, tok_col); continue; }
            if (c == '<' && peek_next() == '=') { advance(); advance(); emit(token_type::le, "<=", tok_line, tok_col); continue; }
            if (c == '>' && peek_next() == '=') { advance(); advance(); emit(token_type::ge, ">=", tok_line, tok_col); continue; }
            if (c == '+' && peek_next() == '=') { advance(); advance(); emit(token_type::plus_assign, "+=", tok_line, tok_col); continue; }
            if (c == '-' && peek_next() == '=') { advance(); advance(); emit(token_type::minus_assign, "-=", tok_line, tok_col); continue; }
            if (c == '*' && peek_next() == '=') { advance(); advance(); emit(token_type::star_assign, "*=", tok_line, tok_col); continue; }
            if (c == '/' && peek_next() == '=') { advance(); advance(); emit(token_type::slash_assign, "/=", tok_line, tok_col); continue; }
            if (c == '%' && peek_next() == '=') { advance(); advance(); emit(token_type::percent_assign, "%=", tok_line, tok_col); continue; }

            // Single-character operators and delimiters
            advance();
            switch (c) {
                case '+': emit(token_type::plus, "+", tok_line, tok_col); break;
                case '-': emit(token_type::minus, "-", tok_line, tok_col); break;
                case '*': emit(token_type::star, "*", tok_line, tok_col); break;
                case '/': emit(token_type::slash, "/", tok_line, tok_col); break;
                case '%': emit(token_type::percent, "%", tok_line, tok_col); break;
                case '=': emit(token_type::assign, "=", tok_line, tok_col); break;
                case '<': emit(token_type::lt, "<", tok_line, tok_col); break;
                case '>': emit(token_type::gt, ">", tok_line, tok_col); break;
                case '(': emit(token_type::lparen, "(", tok_line, tok_col); break;
                case ')': emit(token_type::rparen, ")", tok_line, tok_col); break;
                case '[': emit(token_type::lbracket, "[", tok_line, tok_col); break;
                case ']': emit(token_type::rbracket, "]", tok_line, tok_col); break;
                case ',': emit(token_type::comma, ",", tok_line, tok_col); break;
                case ':': emit(token_type::colon, ":", tok_line, tok_col); break;
                case '.': emit(token_type::dot, ".", tok_line, tok_col); break;
                default:
                    error(std::string("unexpected character '") + c + "'");
                    break;
            }
        }

        // Close remaining indents
        while (indent_stack.size() > 1) {
            indent_stack.pop_back();
            emit(token_type::dedent, "", line, 1);
        }

        // Ensure final newline
        if (!tokens.empty() && tokens.back().type != token_type::newline) {
            emit(token_type::newline, "", line, column);
        }

        emit(token_type::eof, "", line, column);
    }
};

} // anonymous namespace

lex_result lex(const std::string& source) {
    lexer_state state(source);
    state.lex_all();
    return {std::move(state.tokens), std::move(state.errors), state.errors.empty()};
}

} // namespace mv
