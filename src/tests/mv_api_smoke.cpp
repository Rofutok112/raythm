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
    const std::vector<float> waveform = {0.2f, 0.4f, 0.8f, 0.3f};
    const std::vector<float> oscilloscope = {-0.5f, 0.0f, 0.5f};
    input.waveform = &waveform;
    input.oscilloscope = &oscilloscope;
    input.level = 0.8f;
    input.waveform_index = 2;

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

    // Check ctx.audio.buffers / analysis
    auto audio_val = ctx->get_attr("audio");
    auto audio = std::get<std::shared_ptr<mv::mv_object>>(audio_val);
    auto buffers_val = audio->get_attr("buffers");
    auto buffers = std::get<std::shared_ptr<mv::mv_object>>(buffers_val);
    auto spec_val = buffers->get_attr("spectrum");
    auto spec = std::get<std::shared_ptr<mv::mv_list>>(spec_val);
    ASSERT(spec->elements.size() == 3);

    auto waveform_val = buffers->get_attr("waveform");
    auto waveform_list = std::get<std::shared_ptr<mv::mv_list>>(waveform_val);
    ASSERT(waveform_list->elements.size() == 4);
    ASSERT(std::get<double>(buffers->get_attr("waveform_size")) == 4.0);
    ASSERT(std::get<double>(buffers->get_attr("waveform_index")) == 2.0);

    auto analysis_val = audio->get_attr("analysis");
    auto analysis = std::get<std::shared_ptr<mv::mv_object>>(analysis_val);
    ASSERT(std::abs(std::get<double>(analysis->get_attr("level")) - 0.8) < 0.0001);

    auto osc_val = buffers->get_attr("oscilloscope");
    auto osc = std::get<std::shared_ptr<mv::mv_list>>(osc_val);
    ASSERT(osc->elements.size() == 3);
    ASSERT(std::get<double>(buffers->get_attr("oscilloscope_size")) == 3.0);
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
    r = DrawRect(x=10, y=20, w=100, h=50, fill="#ff0000")
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
    r = DrawRect(x=0, y=0, w=10, h=10, fill=c)
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
    bg = DrawBackground(fill="#111111")
    line = DrawLine(x1=0, y1=0, x2=100, y2=100, thickness=3)
    txt = DrawText(text="hello", x=50, y=50, font_size=24)
    circ = DrawCircle(cx=200, cy=200, radius=30)
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

TEST(test_polyline_node_construction) {
    const char* src = R"(
def draw(ctx):
    pts = [Point(x=0, y=0), Point(x=50, y=25), Point(x=100, y=0)]
    DrawPolyline(points=pts, stroke="#22ddaa", thickness=3.0, opacity=0.5)
)";
    mv::mv_runtime rt;
    ASSERT(rt.load_source(src));

    mv::context_input input;
    auto scene = rt.tick(input);
    ASSERT(scene.has_value());
    ASSERT(scene->nodes.size() == 1);
    auto* poly = std::get_if<mv::polyline_node>(&scene->nodes[0]);
    ASSERT(poly != nullptr);
    ASSERT(poly->points.size() == 3);
    ASSERT(std::abs(poly->points[1].y - 25.0f) < 0.01f);
    ASSERT(std::abs(poly->thickness - 3.0f) < 0.01f);
}

TEST(test_list_append_method_builds_polyline_points) {
    const char* src = R"(
def draw(ctx):
    pts = []
    pts.append(Point(x=0, y=0))
    pts.append(Point(x=25, y=50))
    pts.append(Point(x=50, y=0))
    DrawPolyline(points=pts, stroke="#22ddaa", thickness=2.0, opacity=0.5)
)";
    mv::mv_runtime rt;
    ASSERT(rt.load_source(src));

    mv::context_input input;
    auto scene = rt.tick(input);
    ASSERT(scene.has_value());
    ASSERT(scene->nodes.size() == 1);
    auto* poly = std::get_if<mv::polyline_node>(&scene->nodes[0]);
    ASSERT(poly != nullptr);
    ASSERT(poly->points.size() == 3);
    ASSERT(std::abs(poly->points[1].x - 25.0f) < 0.01f);
    ASSERT(std::abs(poly->points[1].y - 50.0f) < 0.01f);
}

TEST(test_ctx_access_in_draw) {
    const char* src = R"(
def draw(ctx):
    bpm = ctx.time.bpm
    beat = ctx.time.beat_phase
    r = DrawRect(x=bpm, y=beat * 100, w=50, h=50)
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

TEST(test_ctx_audio_waveform_access_in_draw) {
    const char* src = R"(
def draw(ctx):
    x = ctx.audio.analysis.level * 100
    y = ctx.audio.buffers.waveform[ctx.audio.buffers.waveform_index] * 100
    DrawRect(x=x, y=y, w=20, h=20, fill="#ffffff")
)";
    mv::mv_runtime rt;
    ASSERT(rt.load_source(src));

    const std::vector<float> waveform = {0.1f, 0.25f, 0.75f};
    mv::context_input input;
    input.waveform = &waveform;
    input.level = 0.75f;
    input.waveform_index = 2;

    auto scene = rt.tick(input);
    ASSERT(scene.has_value());
    ASSERT(scene->nodes.size() == 1);
    auto* rect = std::get_if<mv::rect_node>(&scene->nodes[0]);
    ASSERT(rect != nullptr);
    ASSERT(std::abs(rect->x - 75.0f) < 0.01f);
    ASSERT(std::abs(rect->y - 75.0f) < 0.01f);
}

TEST(test_ctx_audio_oscilloscope_access_in_draw) {
    const char* src = R"(
def draw(ctx):
    x = (ctx.audio.buffers.oscilloscope[0] + 1) * 50
    y = (ctx.audio.buffers.oscilloscope[2] + 1) * 50
    w = ctx.audio.buffers.oscilloscope_size * 10
    DrawRect(x=x, y=y, w=w, h=20, fill="#ffffff")
)";
    mv::mv_runtime rt;
    ASSERT(rt.load_source(src));

    const std::vector<float> oscilloscope = {-0.5f, 0.0f, 0.25f};
    mv::context_input input;
    input.oscilloscope = &oscilloscope;

    auto scene = rt.tick(input);
    ASSERT(scene.has_value());
    ASSERT(scene->nodes.size() == 1);
    auto* rect = std::get_if<mv::rect_node>(&scene->nodes[0]);
    ASSERT(rect != nullptr);
    ASSERT(std::abs(rect->x - 25.0f) < 0.01f);
    ASSERT(std::abs(rect->y - 62.5f) < 0.01f);
    ASSERT(std::abs(rect->w - 30.0f) < 0.01f);
}

TEST(test_imperative_draw_without_return) {
    const char* src = R"(
def draw(ctx):
    DrawBackground(fill="#0a0a1a")
    DrawCircle(cx=640, cy=360, radius=80, fill="#00ccff", opacity=0.8)
)";
    mv::mv_runtime rt;
    ASSERT(rt.load_source(src));

    mv::context_input input;
    auto scene = rt.tick(input);
    ASSERT(scene.has_value());
    ASSERT(scene->nodes.size() == 2);
    ASSERT(std::holds_alternative<mv::background_node>(scene->nodes[0]));
    ASSERT(std::holds_alternative<mv::circle_node>(scene->nodes[1]));

    auto* circle = std::get_if<mv::circle_node>(&scene->nodes[1]);
    ASSERT(circle != nullptr);
    ASSERT(std::abs(circle->radius - 80.0f) < 0.01f);
}

TEST(test_unknown_function_fails_compile) {
    const char* src = R"(
def draw(ctx):
    totally_not_real(123)
)";
    mv::mv_runtime rt;
    ASSERT(!rt.load_source(src));
}

TEST(test_unknown_variable_fails_compile) {
    const char* src = R"(
def draw(ctx):
    a.get()
)";
    mv::mv_runtime rt;
    ASSERT(!rt.load_source(src));
}

TEST(test_imperative_progress_bar_draw) {
    const char* src = R"(
def draw(ctx):
    bar_w = ctx.screen.w - 100
    filled = bar_w * ctx.time.progress

    DrawRect(x=50, y=680, w=bar_w, h=6, fill="#333333")
    DrawRect(x=50, y=680, w=filled, h=6, fill="#00ff88")

    pct = str(int(ctx.time.progress * 100)) + "%"
    DrawText(text=pct, x=50, y=656, font_size=16, fill="#aaaaaa")
)";
    mv::mv_runtime rt;
    ASSERT(rt.load_source(src));

    mv::context_input input;
    input.current_ms = 60000.0;
    input.song_length_ms = 120000.0;
    input.screen_w = 1280.0f;
    auto scene = rt.tick(input);
    ASSERT(scene.has_value());
    ASSERT(scene->nodes.size() == 3);
    ASSERT(std::holds_alternative<mv::rect_node>(scene->nodes[0]));
    ASSERT(std::holds_alternative<mv::rect_node>(scene->nodes[1]));
    ASSERT(std::holds_alternative<mv::text_node>(scene->nodes[2]));

    auto* bg_bar = std::get_if<mv::rect_node>(&scene->nodes[0]);
    auto* fill_bar = std::get_if<mv::rect_node>(&scene->nodes[1]);
    auto* pct = std::get_if<mv::text_node>(&scene->nodes[2]);
    ASSERT(bg_bar != nullptr);
    ASSERT(fill_bar != nullptr);
    ASSERT(pct != nullptr);
    ASSERT(std::abs(bg_bar->w - 1180.0f) < 0.01f);
    ASSERT(std::abs(fill_bar->w - 590.0f) < 0.01f);
    ASSERT(pct->text == "50%");
}

TEST(test_manual_spectrum_scene_from_primitives) {
    const char* src = R"(
def draw(ctx):
    DrawBackground(fill="#0b0f18")

    count = min(len(ctx.audio.buffers.spectrum), 4)
    if count > 0:
        bar_w = 200 / count
        for i in range(count):
            amp = ctx.audio.buffers.spectrum[i]
            h = 100 * amp
            DrawRect(x=100 + i * bar_w, y=200 - h, w=bar_w - 2, h=h, fill="#64c8ff", opacity=0.7)
)";
    mv::mv_runtime rt;
    ASSERT(rt.load_source(src));

    mv::context_input input;
    input.spectrum = {0.1f, 0.3f, 0.6f, 0.9f, 1.0f};

    auto scene = rt.tick(input);
    ASSERT(scene.has_value());
    ASSERT(scene->nodes.size() == 5);
    ASSERT(std::holds_alternative<mv::background_node>(scene->nodes[0]));

    auto* bar0 = std::get_if<mv::rect_node>(&scene->nodes[1]);
    auto* bar3 = std::get_if<mv::rect_node>(&scene->nodes[4]);
    ASSERT(bar0 != nullptr);
    ASSERT(bar3 != nullptr);
    ASSERT(std::abs(bar0->h - 10.0f) < 0.01f);
    ASSERT(std::abs(bar3->h - 90.0f) < 0.01f);
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
        nodes = nodes + [DrawRect(x=i * 10, y=0, w=10, h=10)]
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

TEST(test_fast_and_generic_drawrect_paths_match) {
    const char* src = R"(
def draw(ctx):
    fast = DrawRect(x=12, y=34, w=56, h=78, fill="#112233", opacity=0.5)
    ctor = DrawRect
    slow = ctor(x=12, y=34, w=56, h=78, fill="#112233", opacity=0.5)
    return Scene([fast, slow])
)";
    mv::mv_runtime rt;
    ASSERT(rt.load_source(src));

    mv::context_input input;
    auto scene = rt.tick(input);
    ASSERT(scene.has_value());
    ASSERT(scene->nodes.size() == 2);

    const auto* fast = std::get_if<mv::rect_node>(&scene->nodes[0]);
    const auto* slow = std::get_if<mv::rect_node>(&scene->nodes[1]);
    ASSERT(fast != nullptr);
    ASSERT(slow != nullptr);
    ASSERT(std::abs(fast->x - slow->x) < 0.001f);
    ASSERT(std::abs(fast->y - slow->y) < 0.001f);
    ASSERT(std::abs(fast->w - slow->w) < 0.001f);
    ASSERT(std::abs(fast->h - slow->h) < 0.001f);
    ASSERT(std::abs(fast->opacity - slow->opacity) < 0.001f);
    ASSERT(fast->fill.r == slow->fill.r);
    ASSERT(fast->fill.g == slow->fill.g);
    ASSERT(fast->fill.b == slow->fill.b);
}

TEST(test_negative_range_loop_builds_nodes) {
    const char* src = R"(
def draw(ctx):
    nodes = []
    for i in range(5, 0, -2):
        nodes = nodes + [DrawRect(x=i, y=0, w=1, h=1)]
    return Scene(nodes)
)";
    mv::mv_runtime rt;
    ASSERT(rt.load_source(src));

    mv::context_input input;
    auto scene = rt.tick(input);
    ASSERT(scene.has_value());
    ASSERT(scene->nodes.size() == 3);
    auto* r0 = std::get_if<mv::rect_node>(&scene->nodes[0]);
    auto* r1 = std::get_if<mv::rect_node>(&scene->nodes[1]);
    auto* r2 = std::get_if<mv::rect_node>(&scene->nodes[2]);
    ASSERT(r0 && std::abs(r0->x - 5.0f) < 0.01f);
    ASSERT(r1 && std::abs(r1->x - 3.0f) < 0.01f);
    ASSERT(r2 && std::abs(r2->x - 1.0f) < 0.01f);
}

TEST(test_runtime_validation_toggle) {
    const char* src = R"(
def draw(ctx):
    return Scene([DrawRect(x=99999, y=0, w=-5, h=10, fill="#ffffff")])
)";
    mv::mv_runtime rt;
    ASSERT(rt.load_source(src));

    mv::context_input input;
    auto raw_scene = rt.tick(input);
    ASSERT(raw_scene.has_value());
    auto* raw_rect = std::get_if<mv::rect_node>(&raw_scene->nodes[0]);
    ASSERT(raw_rect != nullptr);
    ASSERT(raw_rect->x > 90000.0f);
    ASSERT(raw_rect->w < 0.0f);

    rt.set_validation_enabled(true);
    auto validated_scene = rt.tick(input);
    ASSERT(validated_scene.has_value());
    auto* validated_rect = std::get_if<mv::rect_node>(&validated_scene->nodes[0]);
    ASSERT(validated_rect != nullptr);
    ASSERT(validated_rect->x == 4096.0f);
    ASSERT(validated_rect->w == 0.0f);
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
    RUN(test_polyline_node_construction);
    RUN(test_list_append_method_builds_polyline_points);
    RUN(test_ctx_access_in_draw);
    RUN(test_ctx_audio_waveform_access_in_draw);
    RUN(test_ctx_audio_oscilloscope_access_in_draw);
    RUN(test_imperative_draw_without_return);
    RUN(test_unknown_function_fails_compile);
    RUN(test_unknown_variable_fails_compile);
    RUN(test_imperative_progress_bar_draw);
    RUN(test_manual_spectrum_scene_from_primitives);
    RUN(test_validator_truncates_nodes);
    RUN(test_validator_sanitizes_nan);
    RUN(test_scene_clear_color);
    RUN(test_runtime_no_draw_function);
    RUN(test_for_loop_builds_nodes);
    RUN(test_fast_and_generic_drawrect_paths_match);
    RUN(test_negative_range_loop_builds_nodes);
    RUN(test_runtime_validation_toggle);

    std::printf("\n%d passed, %d failed\n", tests_passed, tests_failed);
    return tests_failed > 0 ? 1 : 0;
}

