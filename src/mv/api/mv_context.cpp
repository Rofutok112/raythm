#include "mv_context.h"

namespace mv {

namespace {

std::shared_ptr<mv_object> make_obj(const std::string& type) {
    auto o = std::make_shared<mv_object>();
    o->type_name = type;
    return o;
}

} // anonymous namespace

std::shared_ptr<mv_object> build_context(const context_input& input) {
    auto ctx = make_obj("ctx");

    // ctx.time
    auto time = make_obj("time");
    time->set_attr("ms", input.current_ms);
    time->set_attr("sec", input.current_ms / 1000.0);
    time->set_attr("length_ms", input.song_length_ms);
    time->set_attr("bpm", static_cast<double>(input.bpm));
    time->set_attr("beat", static_cast<double>(input.beat_number));
    time->set_attr("beat_phase", static_cast<double>(input.beat_phase));
    // normalized progress 0..1
    double progress = (input.song_length_ms > 0)
        ? input.current_ms / input.song_length_ms
        : 0.0;
    time->set_attr("progress", progress);
    ctx->set_attr("time", mv_value{time});

    // ctx.audio
    auto audio = make_obj("audio");
    auto spec_list = std::make_shared<mv_list>();
    spec_list->elements.reserve(input.spectrum.size());
    for (float v : input.spectrum) {
        spec_list->elements.push_back(static_cast<double>(v));
    }
    audio->set_attr("spectrum", mv_value{spec_list});
    audio->set_attr("spectrum_size", static_cast<double>(input.spectrum.size()));
    ctx->set_attr("audio", mv_value{audio});

    // ctx.chart
    auto chart = make_obj("chart");
    chart->set_attr("total_notes", static_cast<double>(input.total_notes));
    chart->set_attr("combo", static_cast<double>(input.combo));
    chart->set_attr("accuracy", static_cast<double>(input.accuracy));
    chart->set_attr("key_count", static_cast<double>(input.key_count));
    ctx->set_attr("chart", mv_value{chart});

    // ctx.screen
    auto screen = make_obj("screen");
    screen->set_attr("w", static_cast<double>(input.screen_w));
    screen->set_attr("h", static_cast<double>(input.screen_h));
    ctx->set_attr("screen", mv_value{screen});

    return ctx;
}

} // namespace mv
