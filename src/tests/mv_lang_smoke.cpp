#include <cstdlib>
#include <cmath>
#include <iostream>
#include <string>

#include "mv/lang/mv_sandbox.h"

namespace {

int tests_passed = 0;
int tests_failed = 0;

void check(bool condition, const std::string& name) {
    if (condition) {
        tests_passed++;
    } else {
        tests_failed++;
        std::cerr << "FAIL: " << name << '\n';
    }
}

void test_basic_arithmetic() {
    mv::sandbox sb;
    sb.set_limits({1000000, 32, 1024, 1024});

    std::string source = R"(
def draw(ctx):
    return 1 + 2
)";

    auto result = sb.run_draw(source, std::monostate{});
    check(result.success, "basic arithmetic: compilation + execution");
    if (result.success) {
        auto* val = std::get_if<double>(&result.value);
        check(val != nullptr && *val == 3.0, "basic arithmetic: 1 + 2 == 3");
    }
}

void test_complex_arithmetic() {
    mv::sandbox sb;
    sb.set_limits({1000000, 32, 1024, 1024});

    std::string source = R"(
def draw(ctx):
    x = 10.0
    y = 3.0
    return (x * y + 5) / 7.0
)";

    auto result = sb.run_draw(source, std::monostate{});
    check(result.success, "complex arithmetic: success");
    if (result.success) {
        auto* val = std::get_if<double>(&result.value);
        check(val != nullptr && std::abs(*val - 5.0) < 0.001, "complex arithmetic: (10*3+5)/7 == 5");
    }
}

void test_private_function() {
    mv::sandbox sb;
    sb.set_limits({1000000, 32, 1024, 1024});

    std::string source = R"(
def pulse(x):
    if x < 0.0:
        return 0.0
    result = 1.0 - x
    if result < 0.0:
        return 0.0
    return result

def draw(ctx):
    return pulse(0.3)
)";

    auto result = sb.run_draw(source, std::monostate{});
    check(result.success, "private function: success");
    if (result.success) {
        auto* val = std::get_if<double>(&result.value);
        check(val != nullptr && std::abs(*val - 0.7) < 0.001, "private function: pulse(0.3) == 0.7");
    }
}

void test_ctx_object() {
    mv::sandbox sb;
    sb.set_limits({1000000, 32, 1024, 1024});

    // Build ctx with nested objects
    auto time_obj = std::make_shared<mv::mv_object>();
    time_obj->type_name = "time";
    time_obj->set_attr("beat", 2.5);
    time_obj->set_attr("now", 1000.0);

    auto ctx = std::make_shared<mv::mv_object>();
    ctx->type_name = "ctx";
    ctx->set_attr("time", mv::mv_value{time_obj});

    std::string source = R"(
def draw(ctx):
    return ctx.time.beat * 2.0
)";

    auto result = sb.run_draw(source, mv::mv_value{ctx});
    check(result.success, "ctx object: success");
    if (result.success) {
        auto* val = std::get_if<double>(&result.value);
        check(val != nullptr && *val == 5.0, "ctx object: beat * 2 == 5");
    }
}

void test_forbidden_import() {
    mv::sandbox sb;
    sb.set_limits({1000000, 32, 1024, 1024});

    std::string source = R"(
import os

def draw(ctx):
    return 0
)";

    auto result = sb.run_draw(source, std::monostate{});
    check(!result.success, "forbidden import: rejected");
    check(!result.errors.empty(), "forbidden import: has error message");
}

void test_forbidden_class() {
    mv::sandbox sb;

    std::string source = R"(
class Foo:
    pass

def draw(ctx):
    return 0
)";

    auto result = sb.run_draw(source, std::monostate{});
    check(!result.success, "forbidden class: rejected");
}

void test_step_limit() {
    mv::sandbox sb;
    sb.set_limits({100, 32, 1024, 1024}); // Very low step limit

    std::string source = R"(
def draw(ctx):
    x = 0
    for i in [1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20]:
        x += i
    return x
)";

    auto result = sb.run_draw(source, std::monostate{});
    check(!result.success, "step limit: exceeded");
    if (!result.errors.empty()) {
        bool found_limit_msg = false;
        for (auto& err : result.errors) {
            if (err.message.find("step limit") != std::string::npos) {
                found_limit_msg = true;
            }
        }
        check(found_limit_msg, "step limit: correct error message");
    }
}

void test_native_function() {
    mv::sandbox sb;
    sb.set_limits({1000000, 32, 1024, 1024});

    sb.register_native("max", [](const std::vector<mv::mv_value>& args) -> mv::mv_value {
        if (args.size() == 2) {
            auto* a = std::get_if<double>(&args[0]);
            auto* b = std::get_if<double>(&args[1]);
            if (a && b) return std::max(*a, *b);
        }
        return std::monostate{};
    });

    std::string source = R"(
def draw(ctx):
    return max(3.0, 7.0)
)";

    auto result = sb.run_draw(source, std::monostate{});
    check(result.success, "native function: success");
    if (result.success) {
        auto* val = std::get_if<double>(&result.value);
        check(val != nullptr && *val == 7.0, "native function: max(3, 7) == 7");
    }
}

void test_native_kwargs_function() {
    mv::sandbox sb;
    sb.set_limits({1000000, 32, 1024, 1024});

    sb.register_native_kwargs("Scene", [](
        const std::vector<mv::mv_value>& pos_args,
        const std::vector<std::pair<std::string, mv::mv_value>>& kwargs
    ) -> mv::mv_value {
        auto obj = std::make_shared<mv::mv_object>();
        obj->type_name = "Scene";
        for (auto& [k, v] : kwargs) {
            obj->set_attr(k, v);
        }
        return obj;
    });

    std::string source = R"(
def draw(ctx):
    return Scene(clear="#0b0f18", nodes=[])
)";

    auto result = sb.run_draw(source, std::monostate{});
    check(result.success, "kwargs function: success");
    if (result.success) {
        auto* obj = std::get_if<std::shared_ptr<mv::mv_object>>(&result.value);
        check(obj != nullptr && *obj != nullptr, "kwargs function: returns object");
        if (obj && *obj) {
            auto clear_val = (*obj)->get_attr("clear");
            auto* clear_str = std::get_if<std::string>(&clear_val);
            check(clear_str != nullptr && *clear_str == "#0b0f18", "kwargs function: clear attr");
        }
    }
}

void test_if_elif_else() {
    mv::sandbox sb;
    sb.set_limits({1000000, 32, 1024, 1024});

    std::string source = R"(
def draw(ctx):
    x = 5
    if x > 10:
        return 1
    elif x > 3:
        return 2
    else:
        return 3
)";

    auto result = sb.run_draw(source, std::monostate{});
    check(result.success, "if/elif/else: success");
    if (result.success) {
        auto* val = std::get_if<double>(&result.value);
        check(val != nullptr && *val == 2.0, "if/elif/else: correct branch");
    }
}

void test_for_loop() {
    mv::sandbox sb;
    sb.set_limits({1000000, 32, 1024, 1024});

    std::string source = R"(
def draw(ctx):
    total = 0
    for x in [10, 20, 30]:
        total += x
    return total
)";

    auto result = sb.run_draw(source, std::monostate{});
    check(result.success, "for loop: success");
    if (result.success) {
        auto* val = std::get_if<double>(&result.value);
        check(val != nullptr && *val == 60.0, "for loop: sum == 60");
    }
}

void test_string_concat() {
    mv::sandbox sb;
    sb.set_limits({1000000, 32, 1024, 1024});

    std::string source = R"(
def draw(ctx):
    return "hello" + " " + "world"
)";

    auto result = sb.run_draw(source, std::monostate{});
    check(result.success, "string concat: success");
    if (result.success) {
        auto* val = std::get_if<std::string>(&result.value);
        check(val != nullptr && *val == "hello world", "string concat: result");
    }
}

void test_bool_logic() {
    mv::sandbox sb;
    sb.set_limits({1000000, 32, 1024, 1024});

    std::string source = R"(
def draw(ctx):
    a = True
    b = False
    if a and not b:
        return 1
    return 0
)";

    auto result = sb.run_draw(source, std::monostate{});
    check(result.success, "bool logic: success");
    if (result.success) {
        auto* val = std::get_if<double>(&result.value);
        check(val != nullptr && *val == 1.0, "bool logic: True and not False == True");
    }
}

void test_list_operations() {
    mv::sandbox sb;
    sb.set_limits({1000000, 32, 1024, 1024});

    std::string source = R"(
def draw(ctx):
    items = [10, 20, 30]
    return items[1]
)";

    auto result = sb.run_draw(source, std::monostate{});
    check(result.success, "list ops: success");
    if (result.success) {
        auto* val = std::get_if<double>(&result.value);
        check(val != nullptr && *val == 20.0, "list ops: items[1] == 20");
    }
}

void test_division_by_zero() {
    mv::sandbox sb;
    sb.set_limits({1000000, 32, 1024, 1024});

    std::string source = R"(
def draw(ctx):
    return 1.0 / 0.0
)";

    auto result = sb.run_draw(source, std::monostate{});
    check(!result.success, "division by zero: detected");
}

void test_missing_draw() {
    mv::sandbox sb;

    std::string source = R"(
def helper():
    return 42
)";

    auto result = sb.run_draw(source, std::monostate{});
    check(!result.success, "missing draw: error");
}

void test_top_level_runs_once_per_compile() {
    mv::sandbox sb;
    sb.set_limits({1000000, 32, 1024, 1024});

    int counter = 0;
    sb.register_native("bump", [&](const std::vector<mv::mv_value>&) -> mv::mv_value {
        counter += 1;
        return 0.0;
    });
    sb.register_native("counter", [&](const std::vector<mv::mv_value>&) -> mv::mv_value {
        return static_cast<double>(counter);
    });

    std::string source = R"(
bump()

def draw(ctx):
    return counter()
)";

    check(sb.compile(source), "top-level one-shot: compile");
    auto first = sb.call("draw", {std::monostate{}});
    auto second = sb.call("draw", {std::monostate{}});

    check(first.success, "top-level one-shot: first draw");
    check(second.success, "top-level one-shot: second draw");
    check(counter == 1, "top-level one-shot: native ran once");
    if (first.success) {
        auto* val = std::get_if<double>(&first.value);
        check(val != nullptr && *val == 1.0, "top-level one-shot: first draw sees initialized state");
    }
    if (second.success) {
        auto* val = std::get_if<double>(&second.value);
        check(val != nullptr && *val == 1.0, "top-level one-shot: second draw reuses initialized state");
    }
}

} // anonymous namespace

int main() {
    test_basic_arithmetic();
    test_complex_arithmetic();
    test_private_function();
    test_ctx_object();
    test_forbidden_import();
    test_forbidden_class();
    test_step_limit();
    test_native_function();
    test_native_kwargs_function();
    test_if_elif_else();
    test_for_loop();
    test_string_concat();
    test_bool_logic();
    test_list_operations();
    test_division_by_zero();
    test_missing_draw();
    test_top_level_runs_once_per_compile();

    std::cout << tests_passed << " passed, " << tests_failed << " failed\n";

    if (tests_failed > 0) {
        return EXIT_FAILURE;
    }

    std::cout << "mv_lang smoke test passed\n";
    return EXIT_SUCCESS;
}
