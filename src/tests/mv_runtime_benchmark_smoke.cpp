#include <chrono>
#include <cstdio>
#include <vector>

#include "mv/mv_runtime.h"

namespace {

using clock_type = std::chrono::steady_clock;

double run_case(const char* name,
                const char* source,
                mv::context_input input,
                int iterations) {
    mv::mv_runtime runtime;
    if (!runtime.load_source(source)) {
        std::printf("%s: compile failed\n", name);
        return -1.0;
    }

    const auto start = clock_type::now();
    for (int i = 0; i < iterations; ++i) {
        input.current_ms = static_cast<double>(i) * 16.0;
        input.beat_phase = static_cast<float>((i % 60) / 60.0);
        input.waveform_index = input.waveform == nullptr || input.waveform->empty()
            ? 0
            : (i % static_cast<int>(input.waveform->size()));
        if (runtime.tick_ref(input) == nullptr) {
            std::printf("%s: tick failed at iteration %d\n", name, i);
            return -1.0;
        }
    }
    const auto end = clock_type::now();
    const double total_ms = std::chrono::duration<double, std::milli>(end - start).count();
    const double per_tick_ms = total_ms / static_cast<double>(iterations);
    std::printf("%-24s total=%8.3fms  per_tick=%6.4fms\n", name, total_ms, per_tick_ms);
    return per_tick_ms;
}

} // namespace

int main() {
    std::printf("=== MV Runtime Benchmark Smoke ===\n");

    std::vector<float> spectrum(64);
    std::vector<float> waveform(512);
    std::vector<float> oscilloscope(256);
    for (std::size_t i = 0; i < spectrum.size(); ++i) {
        spectrum[i] = static_cast<float>((i % 8) / 8.0f);
    }
    for (std::size_t i = 0; i < waveform.size(); ++i) {
        waveform[i] = static_cast<float>((i % 64) / 64.0f);
    }
    for (std::size_t i = 0; i < oscilloscope.size(); ++i) {
        oscilloscope[i] = static_cast<float>((static_cast<int>(i % 32) - 16) / 16.0f);
    }

    mv::context_input input;
    input.song_length_ms = 120000.0;
    input.bpm = 160.0f;
    input.beat_number = 32;
    input.spectrum = spectrum;
    input.waveform = &waveform;
    input.oscilloscope = &oscilloscope;
    input.level = 0.5f;
    input.rms = 0.4f;
    input.peak = 0.8f;
    input.low = 0.2f;
    input.mid = 0.5f;
    input.high = 0.7f;
    input.song_id = "song";
    input.song_title = "Benchmark Song";
    input.song_artist = "Codex";
    input.song_base_bpm = 160.0f;
    input.chart_id = "chart";
    input.chart_song_id = "song";
    input.chart_difficulty = "Hard";
    input.chart_level = 12.0f;
    input.chart_author = "Codex";
    input.chart_resolution = 480;
    input.chart_offset = 0;
    input.total_notes = 900;
    input.combo = 123;
    input.accuracy = 0.98f;
    input.key_count = 4;
    input.screen_w = 1280.0f;
    input.screen_h = 720.0f;

    constexpr int kIterations = 1000;

    const char* static_scene = R"(
def draw(ctx):
    bg = DrawBackground(fill="#0b0f18")
    box = DrawRect(x=100, y=100, w=240, h=120, fill="#64c8ff", opacity=0.8)
    label = DrawText(text="benchmark", x=120, y=140, font_size=28, fill="#ffffff")
    return Scene([bg, box, label])
)";

    const char* node_heavy = R"(
def draw(ctx):
    nodes = []
    for i in range(96):
        nodes = nodes + [DrawRect(x=i * 12, y=(i % 8) * 18, w=10, h=10, fill="#ffaa44", opacity=0.6)]
    return Scene(nodes)
)";

    const char* spectrum_bars = R"(
def draw(ctx):
    nodes = [DrawBackground(fill="#101820")]
    count = min(len(ctx.audio.buffers.spectrum), 32)
    if count > 0:
        bar_w = 512 / count
        for i in range(count):
            amp = ctx.audio.buffers.spectrum[i]
            nodes = nodes + [DrawRect(x=100 + i * bar_w, y=400 - amp * 200, w=bar_w - 2, h=amp * 200, fill="#44ddaa")]
    return Scene(nodes)
)";

    const char* oscilloscope_polyline = R"(
def draw(ctx):
    pts = []
    count = min(len(ctx.audio.buffers.oscilloscope), 64)
    if count > 0:
        step_x = 800 / count
        for i in range(count):
            y = 360 + ctx.audio.buffers.oscilloscope[i] * 120
            pts.append(Point(x=200 + i * step_x, y=y))
    bg = DrawBackground(fill="#06090f")
    line = DrawPolyline(points=pts, stroke="#64c8ff", thickness=2.0, opacity=0.9)
    return Scene([bg, line])
)";

    const double static_ms = run_case("static scene", static_scene, input, kIterations);
    const double node_heavy_ms = run_case("node-heavy loop", node_heavy, input, kIterations);
    const double spectrum_ms = run_case("spectrum bars", spectrum_bars, input, kIterations);
    const double oscilloscope_ms = run_case("oscilloscope polyline", oscilloscope_polyline, input, kIterations);

    const bool ok = static_ms >= 0.0 && node_heavy_ms >= 0.0 && spectrum_ms >= 0.0 && oscilloscope_ms >= 0.0;
    return ok ? 0 : 1;
}
