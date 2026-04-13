#include "mv_parser.h"
#include "mv_lexer.h"

#include <sstream>

namespace mv {

namespace {

struct parser_state {
    std::vector<token> tokens;
    int pos = 0;
    std::vector<std::string> errors;
    bool had_error = false;

    const token& current() const { return tokens[pos]; }
    const token& peek() const { return tokens[pos]; }

    token_type peek_type() const { return tokens[pos].type; }

    bool at_end() const { return peek_type() == token_type::eof; }

    const token& advance() {
        const token& t = tokens[pos];
        if (!at_end()) pos++;
        return t;
    }

    bool check(token_type type) const { return peek_type() == type; }

    bool match(token_type type) {
        if (check(type)) { advance(); return true; }
        return false;
    }

    const token& expect(token_type type, const std::string& msg) {
        if (check(type)) return advance();
        error(msg);
        // Return current token anyway to avoid crashing
        return tokens[pos];
    }

    void error(const std::string& msg) {
        const token& t = current();
        std::ostringstream oss;
        oss << "line " << t.line << ":" << t.column << ": " << msg;
        if (t.type != token_type::eof) {
            oss << " (got '" << t.text << "')";
        }
        errors.push_back(oss.str());
        had_error = true;
    }

    source_location loc() const {
        return {current().line, current().column};
    }

    void skip_newlines() {
        while (check(token_type::newline)) advance();
    }

    void synchronize() {
        while (!at_end()) {
            if (check(token_type::newline)) { advance(); return; }
            if (check(token_type::kw_def) || check(token_type::kw_if) ||
                check(token_type::kw_for) || check(token_type::kw_return)) return;
            advance();
        }
    }

    // ---- Expression parsing (precedence climbing) ----

    expr_ptr parse_expr() { return parse_or(); }

    expr_ptr parse_or() {
        auto left = parse_and();
        while (check(token_type::kw_or)) {
            auto l = loc();
            advance();
            auto right = parse_and();
            auto e = std::make_unique<expr>();
            e->loc = l;
            e->kind = binary_expr{binary_op::logical_or, std::move(left), std::move(right)};
            left = std::move(e);
        }
        return left;
    }

    expr_ptr parse_and() {
        auto left = parse_not();
        while (check(token_type::kw_and)) {
            auto l = loc();
            advance();
            auto right = parse_not();
            auto e = std::make_unique<expr>();
            e->loc = l;
            e->kind = binary_expr{binary_op::logical_and, std::move(left), std::move(right)};
            left = std::move(e);
        }
        return left;
    }

    expr_ptr parse_not() {
        if (check(token_type::kw_not)) {
            auto l = loc();
            advance();
            auto operand = parse_not();
            auto e = std::make_unique<expr>();
            e->loc = l;
            e->kind = unary_expr{unary_op::logical_not, std::move(operand)};
            return e;
        }
        return parse_comparison();
    }

    expr_ptr parse_comparison() {
        auto left = parse_addition();
        while (check(token_type::eq) || check(token_type::ne) ||
               check(token_type::lt) || check(token_type::gt) ||
               check(token_type::le) || check(token_type::ge)) {
            auto l = loc();
            auto op_tok = advance();
            binary_op op;
            switch (op_tok.type) {
                case token_type::eq: op = binary_op::eq; break;
                case token_type::ne: op = binary_op::ne; break;
                case token_type::lt: op = binary_op::lt; break;
                case token_type::gt: op = binary_op::gt; break;
                case token_type::le: op = binary_op::le; break;
                case token_type::ge: op = binary_op::ge; break;
                default: op = binary_op::eq; break;
            }
            auto right = parse_addition();
            auto e = std::make_unique<expr>();
            e->loc = l;
            e->kind = binary_expr{op, std::move(left), std::move(right)};
            left = std::move(e);
        }
        return left;
    }

    expr_ptr parse_addition() {
        auto left = parse_multiplication();
        while (check(token_type::plus) || check(token_type::minus)) {
            auto l = loc();
            auto op_tok = advance();
            binary_op op = (op_tok.type == token_type::plus) ? binary_op::add : binary_op::sub;
            auto right = parse_multiplication();
            auto e = std::make_unique<expr>();
            e->loc = l;
            e->kind = binary_expr{op, std::move(left), std::move(right)};
            left = std::move(e);
        }
        return left;
    }

    expr_ptr parse_multiplication() {
        auto left = parse_unary();
        while (check(token_type::star) || check(token_type::slash) || check(token_type::percent)) {
            auto l = loc();
            auto op_tok = advance();
            binary_op op;
            switch (op_tok.type) {
                case token_type::star: op = binary_op::mul; break;
                case token_type::slash: op = binary_op::div; break;
                default: op = binary_op::mod; break;
            }
            auto right = parse_unary();
            auto e = std::make_unique<expr>();
            e->loc = l;
            e->kind = binary_expr{op, std::move(left), std::move(right)};
            left = std::move(e);
        }
        return left;
    }

    expr_ptr parse_unary() {
        if (check(token_type::minus)) {
            auto l = loc();
            advance();
            auto operand = parse_power();
            auto e = std::make_unique<expr>();
            e->loc = l;
            e->kind = unary_expr{unary_op::negate, std::move(operand)};
            return e;
        }
        return parse_power();
    }

    expr_ptr parse_power() {
        auto left = parse_postfix();
        if (check(token_type::power)) {
            auto l = loc();
            advance();
            auto right = parse_unary(); // right-associative
            auto e = std::make_unique<expr>();
            e->loc = l;
            e->kind = binary_expr{binary_op::power, std::move(left), std::move(right)};
            return e;
        }
        return left;
    }

    expr_ptr parse_postfix() {
        auto left = parse_primary();
        while (true) {
            if (check(token_type::lparen)) {
                left = parse_call(std::move(left));
            } else if (check(token_type::dot)) {
                auto l = loc();
                advance();
                const token& attr_tok = expect(token_type::ident, "expected attribute name");
                auto e = std::make_unique<expr>();
                e->loc = l;
                e->kind = attr_expr{std::move(left), attr_tok.text};
                left = std::move(e);
            } else if (check(token_type::lbracket)) {
                auto l = loc();
                advance();
                auto index = parse_expr();
                expect(token_type::rbracket, "expected ']'");
                auto e = std::make_unique<expr>();
                e->loc = l;
                e->kind = index_expr{std::move(left), std::move(index)};
                left = std::move(e);
            } else {
                break;
            }
        }
        return left;
    }

    expr_ptr parse_call(expr_ptr callee) {
        auto l = callee->loc;
        advance(); // consume '('

        std::vector<expr_ptr> positional;
        std::vector<keyword_arg> kwargs;
        bool seen_kwarg = false;

        while (!check(token_type::rparen) && !at_end()) {
            int prev_pos = pos;
            // Check for keyword argument: ident = expr
            if (check(token_type::ident) && pos + 1 < static_cast<int>(tokens.size()) &&
                tokens[pos + 1].type == token_type::assign) {
                std::string name = advance().text;
                advance(); // consume '='
                auto value = parse_expr();
                kwargs.push_back({std::move(name), std::move(value)});
                seen_kwarg = true;
            } else {
                if (seen_kwarg) {
                    error("positional argument after keyword argument");
                }
                positional.push_back(parse_expr());
            }

            if (!check(token_type::rparen)) {
                if (!check(token_type::comma)) {
                    error("expected ',' or ')'");
                    break;
                }
                advance(); // consume ','
            }
            // Safety: prevent infinite loop
            if (pos == prev_pos) break;
        }
        if (check(token_type::rparen)) advance();

        auto e = std::make_unique<expr>();
        e->loc = l;
        if (kwargs.empty()) {
            e->kind = call_expr{std::move(callee), std::move(positional)};
        } else {
            e->kind = call_with_kwargs_expr{std::move(callee), std::move(positional), std::move(kwargs)};
        }
        return e;
    }

    expr_ptr parse_primary() {
        auto l = loc();

        if (check(token_type::number)) {
            const token& number_tok = advance();
            return make_expr<number_literal>(l, number_tok.has_number_value
                                                    ? number_tok.number_value
                                                    : std::stod(number_tok.text));
        }
        if (check(token_type::string_lit)) {
            return make_expr<string_literal>(l, advance().text);
        }
        if (check(token_type::true_lit)) {
            advance();
            return make_expr<bool_literal>(l, true);
        }
        if (check(token_type::false_lit)) {
            advance();
            return make_expr<bool_literal>(l, false);
        }
        if (check(token_type::none_lit)) {
            advance();
            return make_expr<none_literal>(l);
        }
        if (check(token_type::ident)) {
            return make_expr<identifier>(l, advance().text);
        }
        if (check(token_type::lparen)) {
            advance();
            auto inner = parse_expr();
            expect(token_type::rparen, "expected ')'");
            return inner;
        }
        if (check(token_type::lbracket)) {
            advance();
            std::vector<expr_ptr> elements;
            while (!check(token_type::rbracket) && !at_end()) {
                int prev_pos = pos;
                elements.push_back(parse_expr());
                if (!check(token_type::rbracket)) {
                    if (!check(token_type::comma)) {
                        error("expected ',' or ']'");
                        break;
                    }
                    advance(); // consume ','
                }
                if (pos == prev_pos) break;
            }
            if (check(token_type::rbracket)) advance();
            return make_expr<list_expr>(l, std::move(elements));
        }

        error("expected expression");
        advance(); // skip bad token
        return make_expr<none_literal>(l);
    }

    // ---- Statement parsing ----

    std::vector<stmt_ptr> parse_block() {
        if (!check(token_type::colon)) {
            error("expected ':'");
            synchronize();
            return {};
        }
        advance(); // consume ':'
        if (!check(token_type::newline)) {
            error("expected newline after ':'");
            synchronize();
            return {};
        }
        advance(); // consume newline
        if (!check(token_type::indent)) {
            error("expected indented block");
            return {};
        }
        advance(); // consume indent

        std::vector<stmt_ptr> body;
        while (!check(token_type::dedent) && !at_end()) {
            skip_newlines();
            if (check(token_type::dedent) || at_end()) break;
            int prev_pos = pos;
            auto s = parse_stmt();
            if (s) body.push_back(std::move(s));
            // Safety: if no progress was made, advance to prevent infinite loop
            if (pos == prev_pos) {
                advance();
            }
        }

        if (check(token_type::dedent)) advance();
        return body;
    }

    stmt_ptr parse_func_def() {
        auto l = loc();
        advance(); // consume 'def'
        if (!check(token_type::ident)) {
            error("expected function name");
            synchronize();
            auto s = std::make_unique<stmt>();
            s->loc = l;
            s->kind = func_def{"", {}, {}};
            return s;
        }
        std::string name = advance().text;

        if (!check(token_type::lparen)) {
            error("expected '('");
            synchronize();
            auto s = std::make_unique<stmt>();
            s->loc = l;
            s->kind = func_def{std::move(name), {}, {}};
            return s;
        }
        advance(); // consume '('

        std::vector<std::string> params;
        while (!check(token_type::rparen) && !at_end()) {
            if (!check(token_type::ident)) {
                error("expected parameter name");
                synchronize();
                break;
            }
            params.push_back(advance().text);
            if (!check(token_type::rparen)) {
                if (!check(token_type::comma)) {
                    error("expected ',' or ')'");
                    break;
                }
                advance(); // consume ','
            }
        }
        if (check(token_type::rparen)) advance();

        auto body = parse_block();

        auto s = std::make_unique<stmt>();
        s->loc = l;
        s->kind = func_def{std::move(name), std::move(params), std::move(body)};
        return s;
    }

    stmt_ptr parse_if_stmt() {
        auto l = loc();
        advance(); // consume 'if'
        auto condition = parse_expr();
        auto body = parse_block();
        skip_newlines();

        if_branch main{std::move(condition), std::move(body)};
        std::vector<if_branch> elifs;
        std::vector<stmt_ptr> else_body;

        while (check(token_type::kw_elif)) {
            advance();
            auto elif_cond = parse_expr();
            auto elif_body = parse_block();
            skip_newlines();
            elifs.push_back({std::move(elif_cond), std::move(elif_body)});
        }

        if (check(token_type::kw_else)) {
            advance();
            else_body = parse_block();
            skip_newlines();
        }

        auto s = std::make_unique<stmt>();
        s->loc = l;
        s->kind = if_stmt{std::move(main), std::move(elifs), std::move(else_body)};
        return s;
    }

    stmt_ptr parse_for_stmt() {
        auto l = loc();
        advance(); // consume 'for'
        if (!check(token_type::ident)) {
            error("expected loop variable");
            synchronize();
            auto s = std::make_unique<stmt>();
            s->loc = l;
            s->kind = for_stmt{"", make_expr<none_literal>(l), {}};
            return s;
        }
        std::string var_name = advance().text;
        if (!check(token_type::kw_in)) {
            error("expected 'in'");
            synchronize();
            auto s = std::make_unique<stmt>();
            s->loc = l;
            s->kind = for_stmt{std::move(var_name), make_expr<none_literal>(l), {}};
            return s;
        }
        advance(); // consume 'in'
        auto iterable = parse_expr();
        auto body = parse_block();

        auto s = std::make_unique<stmt>();
        s->loc = l;
        s->kind = for_stmt{std::move(var_name), std::move(iterable), std::move(body)};
        return s;
    }

    stmt_ptr parse_return_stmt() {
        auto l = loc();
        advance(); // consume 'return'

        std::optional<expr_ptr> value;
        if (!check(token_type::newline) && !check(token_type::eof) && !check(token_type::dedent)) {
            value = parse_expr();
        }

        auto s = std::make_unique<stmt>();
        s->loc = l;
        s->kind = return_stmt{std::move(value)};
        if (check(token_type::newline)) advance();
        return s;
    }

    binary_op augmented_op(token_type type) {
        switch (type) {
            case token_type::plus_assign: return binary_op::add;
            case token_type::minus_assign: return binary_op::sub;
            case token_type::star_assign: return binary_op::mul;
            case token_type::slash_assign: return binary_op::div;
            case token_type::percent_assign: return binary_op::mod;
            default: return binary_op::add;
        }
    }

    bool is_augmented_assign(token_type type) {
        return type == token_type::plus_assign || type == token_type::minus_assign ||
               type == token_type::star_assign || type == token_type::slash_assign ||
               type == token_type::percent_assign;
    }

    stmt_ptr parse_expr_or_assign_stmt() {
        auto l = loc();
        auto lhs = parse_expr();

        // Simple assignment: name = expr
        if (check(token_type::assign)) {
            advance();
            auto value = parse_expr();

            if (auto* id = std::get_if<identifier>(&lhs->kind)) {
                auto s = std::make_unique<stmt>();
                s->loc = l;
                s->kind = assign_stmt{id->name, std::move(value)};
                if (check(token_type::newline)) advance();
                return s;
            }
            if (auto* attr = std::get_if<attr_expr>(&lhs->kind)) {
                auto s = std::make_unique<stmt>();
                s->loc = l;
                s->kind = attr_assign_stmt{std::move(attr->object), attr->attr, std::move(value)};
                if (check(token_type::newline)) advance();
                return s;
            }
            if (auto* idx = std::get_if<index_expr>(&lhs->kind)) {
                auto s = std::make_unique<stmt>();
                s->loc = l;
                s->kind = index_assign_stmt{std::move(idx->object), std::move(idx->index), std::move(value)};
                if (check(token_type::newline)) advance();
                return s;
            }
            error("invalid assignment target");
        }

        // Augmented assignment: name += expr
        if (is_augmented_assign(peek_type())) {
            auto op = augmented_op(advance().type);
            auto value = parse_expr();

            if (auto* id = std::get_if<identifier>(&lhs->kind)) {
                auto s = std::make_unique<stmt>();
                s->loc = l;
                s->kind = augmented_assign_stmt{id->name, op, std::move(value)};
                if (check(token_type::newline)) advance();
                return s;
            }
            error("augmented assignment requires a simple variable");
        }

        // Expression statement
        auto s = std::make_unique<stmt>();
        s->loc = l;
        s->kind = expr_stmt{std::move(lhs)};
        if (check(token_type::newline)) advance();
        return s;
    }

    stmt_ptr parse_stmt() {
        if (had_error && errors.size() > 20) return nullptr;

        switch (peek_type()) {
            case token_type::kw_def: return parse_func_def();
            case token_type::kw_if: return parse_if_stmt();
            case token_type::kw_for: return parse_for_stmt();
            case token_type::kw_return: return parse_return_stmt();
            default: return parse_expr_or_assign_stmt();
        }
    }

    program parse_program() {
        program prog;
        skip_newlines();
        while (!at_end()) {
            if (had_error && errors.size() > 20) break;
            int prev_pos = pos;
            auto s = parse_stmt();
            if (s) prog.statements.push_back(std::move(s));
            skip_newlines();
            // Safety: if no progress was made, advance to prevent infinite loop
            if (pos == prev_pos) {
                advance();
            }
        }
        return prog;
    }
};

} // anonymous namespace

parse_result parse(const std::string& source) {
    lex_result lr = lex(source);
    if (!lr.success) {
        return {{}, std::move(lr.errors), false};
    }

    parser_state state;
    state.tokens = std::move(lr.tokens);
    auto prog = state.parse_program();
    return {std::move(prog), std::move(state.errors), !state.had_error};
}

} // namespace mv
