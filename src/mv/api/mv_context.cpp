#include "mv_context.h"

namespace mv {

namespace {

ctx_root_object& as_ctx_root(const std::shared_ptr<mv_object>& object) {
    return *static_cast<ctx_root_object*>(object.get());
}

ctx_time_object& as_ctx_time(const std::shared_ptr<mv_object>& object) {
    return *static_cast<ctx_time_object*>(object.get());
}

ctx_audio_object& as_ctx_audio(const std::shared_ptr<mv_object>& object) {
    return *static_cast<ctx_audio_object*>(object.get());
}

ctx_audio_analysis_object& as_ctx_audio_analysis(const std::shared_ptr<mv_object>& object) {
    return *static_cast<ctx_audio_analysis_object*>(object.get());
}

ctx_audio_bands_object& as_ctx_audio_bands(const std::shared_ptr<mv_object>& object) {
    return *static_cast<ctx_audio_bands_object*>(object.get());
}

ctx_audio_buffers_object& as_ctx_audio_buffers(const std::shared_ptr<mv_object>& object) {
    return *static_cast<ctx_audio_buffers_object*>(object.get());
}

ctx_song_object& as_ctx_song(const std::shared_ptr<mv_object>& object) {
    return *static_cast<ctx_song_object*>(object.get());
}

ctx_chart_object& as_ctx_chart(const std::shared_ptr<mv_object>& object) {
    return *static_cast<ctx_chart_object*>(object.get());
}

ctx_screen_object& as_ctx_screen(const std::shared_ptr<mv_object>& object) {
    return *static_cast<ctx_screen_object*>(object.get());
}

} // anonymous namespace

context_builder::context_builder()
    : ctx_(std::make_shared<ctx_root_object>()),
      time_(std::make_shared<ctx_time_object>()),
      audio_(std::make_shared<ctx_audio_object>()),
      audio_analysis_(std::make_shared<ctx_audio_analysis_object>()),
      audio_bands_(std::make_shared<ctx_audio_bands_object>()),
      audio_buffers_(std::make_shared<ctx_audio_buffers_object>()),
      song_(std::make_shared<ctx_song_object>()),
      chart_(std::make_shared<ctx_chart_object>()),
      screen_(std::make_shared<ctx_screen_object>()),
      spectrum_(std::make_shared<mv_list>()),
      waveform_(std::make_shared<mv_list>()),
      oscilloscope_(std::make_shared<mv_list>()) {
    auto& ctx = as_ctx_root(ctx_);
    ctx.time = time_;
    ctx.audio = audio_;
    ctx.song = song_;
    ctx.chart = chart_;
    ctx.screen = screen_;

    auto& audio = as_ctx_audio(audio_);
    audio.analysis = audio_analysis_;
    audio.bands = audio_bands_;
    audio.buffers = audio_buffers_;

    auto& buffers = as_ctx_audio_buffers(audio_buffers_);
    buffers.spectrum = spectrum_;
    buffers.waveform = waveform_;
    buffers.oscilloscope = oscilloscope_;
}

std::shared_ptr<mv_object> context_builder::build(const context_input& input) {
    auto& time = as_ctx_time(time_);
    time.ms = input.current_ms;
    time.sec = input.current_ms / 1000.0;
    time.length_ms = input.song_length_ms;
    time.bpm = static_cast<double>(input.bpm);
    time.beat = static_cast<double>(input.beat_number);
    time.beat_phase = static_cast<double>(input.beat_phase);
    time.meter_numerator = static_cast<double>(input.meter_numerator);
    time.meter_denominator = static_cast<double>(input.meter_denominator);
    const double progress = (input.song_length_ms > 0)
        ? input.current_ms / input.song_length_ms
        : 0.0;
    time.progress = progress;

    spectrum_->elements.resize(input.spectrum.size());
    for (std::size_t i = 0; i < input.spectrum.size(); ++i) {
        spectrum_->elements[i] = static_cast<double>(input.spectrum[i]);
    }
    auto& analysis = as_ctx_audio_analysis(audio_analysis_);
    analysis.level = static_cast<double>(input.level);
    analysis.rms = static_cast<double>(input.rms);
    analysis.peak = static_cast<double>(input.peak);

    auto& bands = as_ctx_audio_bands(audio_bands_);
    bands.low = static_cast<double>(input.low);
    bands.mid = static_cast<double>(input.mid);
    bands.high = static_cast<double>(input.high);

    auto& buffers = as_ctx_audio_buffers(audio_buffers_);
    buffers.spectrum_size = static_cast<double>(input.spectrum.size());

    const std::vector<float>* waveform_source = input.waveform;
    if (waveform_source_ != waveform_source) {
        waveform_source_ = waveform_source;
        waveform_->elements.clear();
        if (waveform_source != nullptr) {
            waveform_->elements.resize(waveform_source->size());
            for (std::size_t i = 0; i < waveform_source->size(); ++i) {
                waveform_->elements[i] = static_cast<double>((*waveform_source)[i]);
            }
        }
    }
    buffers.waveform_size = static_cast<double>(waveform_source != nullptr ? waveform_source->size() : 0);
    buffers.waveform_index = static_cast<double>(input.waveform_index);

    if (input.oscilloscope != nullptr) {
        oscilloscope_->elements.resize(input.oscilloscope->size());
        for (std::size_t i = 0; i < input.oscilloscope->size(); ++i) {
            oscilloscope_->elements[i] = static_cast<double>((*input.oscilloscope)[i]);
        }
        buffers.oscilloscope_size = static_cast<double>(input.oscilloscope->size());
    } else {
        oscilloscope_->elements.clear();
        buffers.oscilloscope_size = 0.0;
    }

    auto& song = as_ctx_song(song_);
    song.song_id = input.song_id;
    song.title = input.song_title;
    song.artist = input.song_artist;
    song.base_bpm = static_cast<double>(input.song_base_bpm);

    auto& chart = as_ctx_chart(chart_);
    chart.chart_id = input.chart_id;
    chart.song_id = input.chart_song_id;
    chart.difficulty = input.chart_difficulty;
    chart.level = static_cast<double>(input.chart_level);
    chart.chart_author = input.chart_author;
    chart.resolution = static_cast<double>(input.chart_resolution);
    chart.offset = static_cast<double>(input.chart_offset);
    chart.total_notes = static_cast<double>(input.total_notes);
    chart.combo = static_cast<double>(input.combo);
    chart.accuracy = static_cast<double>(input.accuracy);
    chart.key_count = static_cast<double>(input.key_count);

    auto& screen = as_ctx_screen(screen_);
    screen.w = static_cast<double>(input.screen_w);
    screen.h = static_cast<double>(input.screen_h);

    return ctx_;
}

std::shared_ptr<mv_object> build_context(const context_input& input) {
    context_builder builder;
    return builder.build(input);
}

} // namespace mv
