//
// Created by rento on 2026/03/08.
//

#pragma once

#include <string>

// 移行期間中の互換ラッパ。
// BASS 初期化責務は audio_manager に寄せ、既存 scene は次段の issue で置換する。
class audio {
public:
    audio();
    ~audio();

    void load(const std::string& file_path);
    void play(bool restart = true) const;
    void pause() const;
    void stop() const;
    void set_volume(float volume) const;
    void set_position_seconds(double seconds) const;
    bool is_loaded() const;
    bool is_playing() const;
    double get_position_seconds() const;
    double get_length_seconds() const;

private:
    unsigned long handle_ = 0;
};
