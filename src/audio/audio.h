//
// Created by rento on 2026/03/08.
//

#pragma once

#include <string>

class audio {
public:
    audio();
    ~audio();

    void load(const std::string& file_path);
    void play(bool restart = true) const;
    void pause() const;
    void stop() const;
    bool is_loaded() const;
    bool is_playing() const;
    double get_position_seconds() const;
    double get_length_seconds() const;

private:
    unsigned long handle_ = 0;
    static int instance_count_;
};
