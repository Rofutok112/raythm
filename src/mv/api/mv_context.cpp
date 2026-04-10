#include "mv_context.h"

namespace mv {

namespace {

std::shared_ptr<mv_object> make_obj(const std::string& type) {
    auto o = std::make_shared<mv_object>();
    o->type_name = type;
    o->reserve_attrs(8);
    return o;
}

} // anonymous namespace

context_builder::context_builder()
    : ctx_(make_obj("ctx")),
      time_(make_obj("time")),
      audio_(make_obj("audio")),
      audio_analysis_(make_obj("audio_analysis")),
      audio_bands_(make_obj("audio_bands")),
      audio_buffers_(make_obj("audio_buffers")),
      song_(make_obj("song")),
      chart_(make_obj("chart")),
      screen_(make_obj("screen")),
      spectrum_(std::make_shared<mv_list>()),
      waveform_(std::make_shared<mv_list>()),
      oscilloscope_(std::make_shared<mv_list>()) {
    ctx_->set_attr("time", mv_value{time_});
    ctx_->set_attr("audio", mv_value{audio_});
    ctx_->set_attr("song", mv_value{song_});
    ctx_->set_attr("chart", mv_value{chart_});
    ctx_->set_attr("screen", mv_value{screen_});
    audio_->set_attr("analysis", mv_value{audio_analysis_});
    audio_->set_attr("bands", mv_value{audio_bands_});
    audio_->set_attr("buffers", mv_value{audio_buffers_});
    audio_buffers_->set_attr("spectrum", mv_value{spectrum_});
    audio_buffers_->set_attr("waveform", mv_value{waveform_});
    audio_buffers_->set_attr("oscilloscope", mv_value{oscilloscope_});
}

std::shared_ptr<mv_object> context_builder::build(const context_input& input) {
    time_->set_attr("ms", input.current_ms);
    time_->set_attr("sec", input.current_ms / 1000.0);
    time_->set_attr("length_ms", input.song_length_ms);
    time_->set_attr("bpm", static_cast<double>(input.bpm));
    time_->set_attr("beat", static_cast<double>(input.beat_number));
    time_->set_attr("beat_phase", static_cast<double>(input.beat_phase));
    time_->set_attr("meter_numerator", static_cast<double>(input.meter_numerator));
    time_->set_attr("meter_denominator", static_cast<double>(input.meter_denominator));

    const double progress = (input.song_length_ms > 0)
        ? input.current_ms / input.song_length_ms
        : 0.0;
    time_->set_attr("progress", progress);

    spectrum_->elements.clear();
    spectrum_->elements.reserve(input.spectrum.size());
    for (float v : input.spectrum) {
        spectrum_->elements.push_back(static_cast<double>(v));
    }
    audio_analysis_->set_attr("level", static_cast<double>(input.level));
    audio_analysis_->set_attr("rms", static_cast<double>(input.rms));
    audio_analysis_->set_attr("peak", static_cast<double>(input.peak));
    audio_bands_->set_attr("low", static_cast<double>(input.low));
    audio_bands_->set_attr("mid", static_cast<double>(input.mid));
    audio_bands_->set_attr("high", static_cast<double>(input.high));
    audio_buffers_->set_attr("spectrum_size", static_cast<double>(input.spectrum.size()));

    const std::vector<float>* waveform_source = input.waveform;
    if (waveform_source_ != waveform_source) {
        waveform_source_ = waveform_source;
        waveform_->elements.clear();
        if (waveform_source != nullptr) {
            waveform_->elements.reserve(waveform_source->size());
            for (float v : *waveform_source) {
                waveform_->elements.push_back(static_cast<double>(v));
            }
        }
    }
    audio_buffers_->set_attr("waveform_size", static_cast<double>(waveform_source != nullptr ? waveform_source->size() : 0));
    audio_buffers_->set_attr("waveform_index", static_cast<double>(input.waveform_index));

    oscilloscope_->elements.clear();
    if (input.oscilloscope != nullptr) {
        oscilloscope_->elements.reserve(input.oscilloscope->size());
        for (float v : *input.oscilloscope) {
            oscilloscope_->elements.push_back(static_cast<double>(v));
        }
        audio_buffers_->set_attr("oscilloscope_size", static_cast<double>(input.oscilloscope->size()));
    } else {
        audio_buffers_->set_attr("oscilloscope_size", 0.0);
    }

    song_->set_attr("song_id", input.song_id);
    song_->set_attr("title", input.song_title);
    song_->set_attr("artist", input.song_artist);
    song_->set_attr("base_bpm", static_cast<double>(input.song_base_bpm));

    chart_->set_attr("chart_id", input.chart_id);
    chart_->set_attr("song_id", input.chart_song_id);
    chart_->set_attr("difficulty", input.chart_difficulty);
    chart_->set_attr("level", static_cast<double>(input.chart_level));
    chart_->set_attr("chart_author", input.chart_author);
    chart_->set_attr("resolution", static_cast<double>(input.chart_resolution));
    chart_->set_attr("offset", static_cast<double>(input.chart_offset));
    chart_->set_attr("total_notes", static_cast<double>(input.total_notes));
    chart_->set_attr("combo", static_cast<double>(input.combo));
    chart_->set_attr("accuracy", static_cast<double>(input.accuracy));
    chart_->set_attr("key_count", static_cast<double>(input.key_count));

    screen_->set_attr("w", static_cast<double>(input.screen_w));
    screen_->set_attr("h", static_cast<double>(input.screen_h));

    return ctx_;
}

std::shared_ptr<mv_object> build_context(const context_input& input) {
    context_builder builder;
    return builder.build(input);
}

} // namespace mv
