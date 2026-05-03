#pragma once

#include <optional>
#include <string>

#include "raylib.h"

namespace square_image_picker {

struct export_options {
    int output_size = 512;
};

struct export_result {
    bool success = false;
    std::string message;
};

class state {
public:
    state() = default;
    ~state();

    state(const state&) = delete;
    state& operator=(const state&) = delete;

    state(state&&) = delete;
    state& operator=(state&&) = delete;

    bool open(const std::string& source_path, std::string& error_message);
    void close();
    bool is_open() const;

    void update();
    void draw() const;

    bool consume_accept();
    bool consume_cancel();

    const std::string& source_path() const;
    export_result export_png(const std::string& destination_path,
                             export_options options = {}) const;

private:
    void unload();
    Rectangle source_crop_rect() const;
    void clamp_center();
    void set_zoom(float zoom);

    bool open_ = false;
    bool accepted_ = false;
    bool canceled_ = false;
    bool dragging_ = false;
    Vector2 drag_offset_ = {};

    std::string source_path_;
    Texture2D texture_ = {};
    bool texture_loaded_ = false;
    int image_width_ = 0;
    int image_height_ = 0;
    Vector2 crop_center_ = {};
    float zoom_ = 1.0f;
};

export_result export_square_png(const std::string& source_path,
                                const std::string& destination_path,
                                Rectangle source_crop,
                                export_options options = {});

export_result export_center_square_png(const std::string& source_path,
                                       const std::string& destination_path,
                                       export_options options = {});

}  // namespace square_image_picker
