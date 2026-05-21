#pragma once

#include <optional>
#include <string_view>

namespace localization {

enum class locale {
    english,
    japanese,
};

enum class text_key {
    settings,
    saved_on_exit,
    saved_on_back,
    settings_hint_tabs,
    settings_hint_back,
    back,
    gameplay,
    gameplay_subtitle,
    audio,
    audio_subtitle,
    video,
    video_subtitle,
    system,
    system_subtitle,
    key_config,
    key_config_subtitle,
    language,
    english,
    japanese,
    note_speed,
    camera_angle,
    lane_width,
    note_height,
    global_offset,
    bgm_volume,
    se_volume,
    loudness_normalization,
    enabled,
    disabled,
    frame_rate,
    unlimited,
    display,
    fullscreen,
    windowed,
    theme,
    dark,
    light,
    mode,
    lane,
    key_already_assigned,
    key_cannot_be_assigned,
    press_a_key,
    home_no_songs,
    jacket,
    notes,
    bpm,
    song_title,
    artist,
    genre,
    audio_file,
    jacket_file,
    browse,
    create,
    creating,
    preview_ms,
    select_audio_file,
    select_image_file_optional,
    crop_image,
    zoom,
    cancel,
    apply,
    mv_metadata,
    mv_metadata_subtitle,
    mv_name,
    author,
    author_name,
    untitled_mv,
    metadata,
    score,
    accuracy,
    all_perfect,
    full_combo,
    failed,
    max_combo,
    avg_offset,
    fast,
    slow,
    result_hint,
    pause_resume,
    pause_retry,
    pause_song_select,
    pause_settings,
    editor_back,
    editor_settings,
};

void set_current_locale(locale value);
locale current_locale();

const char* locale_code(locale value);
const char* locale_display_name(locale value);
std::optional<locale> parse_locale_code(std::string_view code);
locale parse_locale_code_or_default(std::string_view code, locale fallback = locale::english);

const char* tr(text_key key);
const char* tr(text_key key, locale value);
const char* tr_literal(const char* english_literal);
const char* english_text(text_key key);
bool has_translation(text_key key, locale value);
int text_key_count();

}  // namespace localization
