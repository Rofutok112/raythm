#include <cassert>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "mv/api/mv_builtins.h"
#include "mv/api/mv_context.h"
#include "mv/api/mv_scene.h"
#include "mv/render/mv_validator.h"
#include "mv/mv_runtime.h"

static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name) static void name()
#define RUN(name) do { \
    std::printf("  %-40s", #name); \
    try { name(); std::printf("PASS\n"); tests_passed++; } \
    catch (...) { std::printf("FAIL\n"); tests_failed++; } \
} while(0)
#define ASSERT(cond) do { if (!(cond)) { std::printf("FAIL: %s line %d\n", #cond, __LINE__); throw 1; } } while(0)

// ---- Color parsing ----

TEST(test_parse_color_hex6) {
    auto c = mv::parse_color("#ff8040");
    ASSERT(c.r == 255);
    ASSERT(c.g == 128);
    ASSERT(c.b == 64);
    ASSERT(c.a == 255);
}

TEST(test_parse_color_hex8) {
    auto c = mv::parse_color("#ff804080");
    ASSERT(c.r == 255);
    ASSERT(c.g == 128);
    ASSERT(c.b == 64);
    ASSERT(c.a == 128);
}

TEST(test_parse_color_invalid) {
    auto c = mv::parse_color("not-a-color");
    ASSERT(c.r == 255 && c.g == 255 && c.b == 255 && c.a == 255);
}

// ---- Context builder ----

TEST(test_build_context) {
    mv::context_input input;
    input.current_ms = 5000;
    input.song_length_ms = 120000;
    input.bpm = 140.0f;
    input.beat_number = 10;
    input.beat_phase = 0.5f;
    input.combo = 42;
    input.key_count = 7;
    input.total_notes = 500;
    input.spectrum = {0.1f, 0.5f, 0.9f};

    auto ctx = mv::build_context(input);
    ASSERT(ctx != nullptr);
    ASSERT(ctx->type_name == "ctx");

    // Check ctx.time
    auto time_val = ctx->get_attr("time");
    auto time = std::get<std::shared_ptr<mv::mv_object>>(time_val);
    ASSERT(std::get<double>(time->get_attr("ms")) == 5000.0);
    ASSERT(std::get<double>(time->get_attr("bpm")) == 140.0);
    ASSERT(std::get<double>(time->get_attr("beat")) == 10.0);

    // Check ctx.chart
    auto chart_val = ctx->get_attr("chart");
    auto chart = std::get<std::shared_ptr<mv::mv_object>>(chart_val);
    ASSERT(std::get<double>(chart->get_attr("combo")) == 42.0);
    ASSERT(std::get<double>(chart->get_attr("key_count")) == 7.0);

    // Check ctx.audio.spectrum
    auto audio_val = ctx->get_attr("audio");
    auto audio = std::get<std::shared_ptr<mv::mv_object>>(audio_val);
    auto spec_val = audio->get_attr("spectrum");
    auto spec = std::get<std::shared_ptr<mv::mv_list>>(spec_val);
    ASSERT(spec->elements.size() == 3);
}

// ---- Builtins in sandbox ----

TEST(test_builtins_math) {
    const char* src = R"(
def draw(ctx):
    x = sin(0)
    y = cos(0)
    z = clamp(5, 0, 3)
    w = lerp(0, 10, 0.5)
    return [x, y, z, w]
)";
    mv::mv_runtime rt;
    ASSERT(rt.load_source(src));
    mv::context_input input;
    // tick will fail because draw returns a list not a Scene, but compilation succeeds
    ASSERT(rt.is_loaded());
}

TEST(test_builtins_scene_construction) {
    const char* src = R"(
def draw(ctx):
    r = Rect(x=10, y=20, w=100, h=50, fill="#ff0000")
    nodes = [r]
    return Scene(nodes)
)";
    mv::mv_runtime rt;
    ASSERT(rt.load_source(src));

    mv::context_input input;
    auto scene = rt.tick(input);
    ASSERT(scene.has_value());
    ASSERT(scene->nodes.size() == 1);
    auto* rect = std::get_if<mv::rect_node>(&scene->nodes[0]);
    ASSERT(rect != nullptr);
    ASSERT(rect->x == 10.0f);
    ASSERT(rect->y == 20.0f);
    ASSERT(rect->w == 100.0f);
    ASSERT(rect->fill.r == 255);
    ASSERT(rect->fill.g == 0);
}

TEST(test_builtins_rgb_color) {
    const char* src = R"(
def draw(ctx):
    c = rgb(100, 200, 50)
    r = Rect(x=0, y=0, w=10, h=10, fill=c)
    return Scene([r])
)";
    mv::mv_runtime rt;
    ASSERT(rt.load_source(src));

    mv::context_input input;
    auto scene = rt.tick(input);
    ASSERT(scene.has_value());
    ASSERT(scene->nodes.size() == 1);
    auto* rect = std::get_if<mv::rect_node>(&scene->nodes[0]);
    ASSERT(rect != nullptr);
    ASSERT(rect->fill.r == 100);
    ASSERT(rect->fill.g == 200);
    ASSERT(rect->fill.b == 50);
}

TEST(test_builtins_multiple_node_types) {
    const char* src = R"(
def draw(ctx):
    bg = Background(fill="#111111")
    line = Line(x1=0, y1=0, x2=100, y2=100, thickness=3)
    txt = Text(text="hello", x=50, y=50, font_size=24)
    circ = Circle(cx=200, cy=200, radius=30)
    return Scene([bg, line, txt, circ])
)";
    mv::mv_runtime rt;
    ASSERT(rt.load_source(src));

    mv::context_input input;
    auto scene = rt.tick(input);
    ASSERT(scene.has_value());
    ASSERT(scene->nodes.size() == 4);

    ASSERT(std::holds_alternative<mv::background_node>(scene->nodes[0]));
    ASSERT(std::holds_alternative<mv::line_node>(scene->nodes[1]));
    ASSERT(std::holds_alternative<mv::text_node>(scene->nodes[2]));
    ASSERT(std::holds_alternative<mv::circle_node>(scene->nodes[3]));

    auto* txt = std::get_if<mv::text_node>(&scene->nodes[2]);
    ASSERT(txt->text == "hello");
    ASSERT(txt->font_size == 24);
}

TEST(test_ctx_access_in_draw) {
    const char* src = R"(
def draw(ctx):
    bpm = ctx.time.bpm
    beat = ctx.time.beat_phase
    r = Rect(x=bpm, y=beat * 100, w=50, h=50)
    return Scene([r])
)";
    mv::mv_runtime rt;
    ASSERT(rt.load_source(src));

    mv::context_input input;
    input.bpm = 180.0f;
    input.beat_phase = 0.75f;

    auto scene = rt.tick(input);
    ASSERT(scene.has_value());
    ASSERT(scene->nodes.size() == 1);
    auto* rect = std::get_if<mv::rect_node>(&scene->nodes[0]);
    ASSERT(rect != nullptr);
    ASSERT(rect->x == 180.0f);
    ASSERT(std::abs(rect->y - 75.0f) < 0.01f);
}

TEST(test_beat_grid_and_spectrum_bar_scene) {
    const char* src = R"(
def draw(ctx):
    bg = Background(fill="#0b0f18")
    spec = SpectrumBar(x=140, y=520, w=1000, h=180, bar_count=48, fill="#64c8ff", opacity=0.7)
    grid = BeatGrid(stroke="#ffffff1e", thickness=1.0, beat_phase=ctx.time.beat_phase, opacity=0.4)
    return Scene([bg, grid, spec])
)";

    const std::filesystem::path temp_path =
        std::filesystem::temp_directory_path() / "raythm_mv_api_beat_grid_test.rmv";
    {
        std::ofstream ofs(temp_path);
        ofs << src;
    }

    mv::mv_runtime rt;
    ASSERT(rt.load_file(temp_path.string()));

    mv::context_input input;
    input.beat_phase = 0.25f;
    input.spectrum = {0.1f, 0.3f, 0.6f};

    auto scene = rt.tick(input);
    ASSERT(scene.has_value());
    ASSERT(scene->nodes.size() == 3);
    ASSERT(std::holds_alternative<mv::background_node>(scene->nodes[0]));
    ASSERT(std::holds_alternative<mv::beat_grid_node>(scene->nodes[1]));
    ASSERT(std::holds_alternative<mv::spectrum_bar_node>(scene->nodes[2]));

    auto* grid = std::get_if<mv::beat_grid_node>(&scene->nodes[1]);
    ASSERT(grid != nullptr);
    ASSERT(std::abs(grid->beat_phase - 0.25f) < 0.01f);

    auto* spec = std::get_if<mv::spectrum_bar_node>(&scene->nodes[2]);
    ASSERT(spec != nullptr);
    ASSERT(spec->spectrum.size() == 3);
    ASSERT(std::abs(spec->spectrum[1] - 0.3f) < 0.01f);

    std::error_code ec;
    std::filesystem::remove(temp_path, ec);
}

// ---- Validator ----

TEST(test_validator_truncates_nodes) {
    mv::scene sc;
    for (int i = 0; i < 600; i++) {
        sc.nodes.push_back(mv::rect_node{});
    }
    mv::validation_limits limits;
    limits.max_nodes = 100;
    auto result = mv::validate_scene(sc, limits);
    ASSERT(sc.nodes.size() == 100);
    ASSERT(!result.warnings.empty());
}

TEST(test_validator_sanitizes_nan) {
    mv::scene sc;
    mv::rect_node r;
    r.x = std::numeric_limits<float>::quiet_NaN();
    r.w = -10.0f;
    sc.nodes.push_back(r);

    mv::validate_scene(sc);
    auto* rect = std::get_if<mv::rect_node>(&sc.nodes[0]);
    ASSERT(rect->x == 0.0f); // NaN → 0
    ASSERT(rect->w == 0.0f); // negative width → 0
}

// ---- Scene clear_color ----

TEST(test_scene_clear_color) {
    const char* src = R"(
def draw(ctx):
    return Scene([], clear_color="#1a2b3c")
)";
    mv::mv_runtime rt;
    ASSERT(rt.load_source(src));

    mv::context_input input;
    auto scene = rt.tick(input);
    ASSERT(scene.has_value());
    ASSERT(scene->clear_color.r == 0x1a);
    ASSERT(scene->clear_color.g == 0x2b);
    ASSERT(scene->clear_color.b == 0x3c);
}

// ---- Runtime: missing draw ----

TEST(test_runtime_no_draw_function) {
    const char* src = R"(
x = 42
)";
    mv::mv_runtime rt;
    ASSERT(rt.load_source(src));

    mv::context_input input;
    auto scene = rt.tick(input);
    ASSERT(!scene.has_value()); // no draw function → no scene
}

// ---- Range + for loop with nodes ----

TEST(test_for_loop_builds_nodes) {
    const char* src = R"(
def draw(ctx):
    nodes = []
    for i in range(5):
        nodes = nodes + [Rect(x=i * 10, y=0, w=10, h=10)]
    return Scene(nodes)
)";
    mv::mv_runtime rt;
    ASSERT(rt.load_source(src));

    mv::context_input input;
    auto scene = rt.tick(input);
    ASSERT(scene.has_value());
    ASSERT(scene->nodes.size() == 5);
    auto* r0 = std::get_if<mv::rect_node>(&scene->nodes[0]);
    auto* r4 = std::get_if<mv::rect_node>(&scene->nodes[4]);
    ASSERT(r0 && r0->x == 0.0f);
    ASSERT(r4 && r4->x == 40.0f);
}

int main() {
    std::printf("=== MV API Smoke Tests ===\n");

    RUN(test_parse_color_hex6);
    RUN(test_parse_color_hex8);
    RUN(test_parse_color_invalid);
    RUN(test_build_context);
    RUN(test_builtins_math);
    RUN(test_builtins_scene_construction);
    RUN(test_builtins_rgb_color);
    RUN(test_builtins_multiple_node_types);
    RUN(test_ctx_access_in_draw);
    RUN(test_beat_grid_and_spectrum_bar_scene);
    RUN(test_validator_truncates_nodes);
    RUN(test_validator_sanitizes_nan);
    RUN(test_scene_clear_color);
    RUN(test_runtime_no_draw_function);
    RUN(test_for_loop_builds_nodes);

    std::printf("\n%d passed, %d failed\n", tests_passed, tests_failed);
    return tests_failed > 0 ? 1 : 0;
}
