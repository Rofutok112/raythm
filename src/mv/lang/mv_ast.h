#pragma once

#include <memory>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace mv {

struct source_location {
    int line = 0;
    int column = 0;
};

// Forward declarations
struct expr;
struct stmt;

using expr_ptr = std::unique_ptr<expr>;
using stmt_ptr = std::unique_ptr<stmt>;

// ---- Expressions ----

enum class binary_op {
    add, sub, mul, div, mod, power,
    eq, ne, lt, gt, le, ge,
    logical_and, logical_or,
};

enum class unary_op {
    negate, logical_not,
};

struct number_literal {
    double value;
};

struct bool_literal {
    bool value;
};

struct string_literal {
    std::string value;
};

struct none_literal {};

struct identifier {
    std::string name;
};

struct binary_expr {
    binary_op op;
    expr_ptr left;
    expr_ptr right;
};

struct unary_expr {
    unary_op op;
    expr_ptr operand;
};

struct call_expr {
    expr_ptr callee;
    std::vector<expr_ptr> args;
};

struct attr_expr {
    expr_ptr object;
    std::string attr;
};

struct index_expr {
    expr_ptr object;
    expr_ptr index;
};

struct list_expr {
    std::vector<expr_ptr> elements;
};

struct keyword_arg {
    std::string name;
    expr_ptr value;
};

struct call_with_kwargs_expr {
    expr_ptr callee;
    std::vector<expr_ptr> positional_args;
    std::vector<keyword_arg> keyword_args;
};

struct expr {
    source_location loc;
    std::variant<
        number_literal,
        bool_literal,
        string_literal,
        none_literal,
        identifier,
        binary_expr,
        unary_expr,
        call_expr,
        attr_expr,
        index_expr,
        list_expr,
        call_with_kwargs_expr
    > kind;
};

// ---- Statements ----

struct expr_stmt {
    expr_ptr expression;
};

struct assign_stmt {
    std::string name;
    expr_ptr value;
};

struct attr_assign_stmt {
    expr_ptr object;
    std::string attr;
    expr_ptr value;
};

struct index_assign_stmt {
    expr_ptr object;
    expr_ptr index;
    expr_ptr value;
};

struct return_stmt {
    std::optional<expr_ptr> value;
};

struct if_branch {
    expr_ptr condition;
    std::vector<stmt_ptr> body;
};

struct if_stmt {
    if_branch main;
    std::vector<if_branch> elifs;
    std::vector<stmt_ptr> else_body;
};

struct for_stmt {
    std::string var_name;
    expr_ptr iterable;
    std::vector<stmt_ptr> body;
};

struct func_def {
    std::string name;
    std::vector<std::string> params;
    std::vector<stmt_ptr> body;
};

struct augmented_assign_stmt {
    std::string name;
    binary_op op; // add, sub, mul, div, mod
    expr_ptr value;
};

struct stmt {
    source_location loc;
    std::variant<
        expr_stmt,
        assign_stmt,
        attr_assign_stmt,
        index_assign_stmt,
        return_stmt,
        if_stmt,
        for_stmt,
        func_def,
        augmented_assign_stmt
    > kind;
};

// ---- Program ----

struct program {
    std::vector<stmt_ptr> statements;
};

// ---- Helpers ----

template<typename T, typename... Args>
expr_ptr make_expr(source_location loc, Args&&... args) {
    auto e = std::make_unique<expr>();
    e->loc = loc;
    e->kind = T{std::forward<Args>(args)...};
    return e;
}

template<typename T, typename... Args>
stmt_ptr make_stmt(source_location loc, Args&&... args) {
    auto s = std::make_unique<stmt>();
    s->loc = loc;
    s->kind = T{std::forward<Args>(args)...};
    return s;
}

} // namespace mv
