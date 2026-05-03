#include "shared/square_image_picker.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <string>

#include "path_utils.h"
#include "scene_common.h"
#include "theme.h"
#include "ui_draw.h"
#include "ui_hit.h"
#include "virtual_screen.h"

namespace square_image_picker {
namespace {

constexpr Rectangle kOverlayRect = {0.0f, 0.0f, static_cast<float>(kScreenWidth), static_cast<float>(kScreenHeight)};
constexpr Rectangle kDialogRect = {455.0f, 86.0f, 1010.0f, 908.0f};
constexpr Rectangle kTitleRect = {501.0f, 126.0f, 540.0f, 40.0f};
constexpr Rectangle kPreviewRect = {565.0f, 188.0f, 790.0f, 630.0f};
constexpr Rectangle kZoomRowRect = {565.0f, 842.0f, 790.0f, 54.0f};
constexpr Rectangle kCancelRect = {1067.0f, 918.0f, 132.0f, 48.0f};
constexpr Rectangle kApplyRect = {1215.0f, 918.0f, 140.0f, 48.0f};
constexpr float kMinZoom = 1.0f;
constexpr float kMaxZoom = 4.0f;

Rectangle fit_rect(Rectangle bounds, int width, int height) {
    if (width <= 0 || height <= 0) {
        return bounds;
    }

    const float scale = std::min(bounds.width / static_cast<float>(width),
                                 bounds.height / static_cast<float>(height));
    const float draw_w = static_cast<float>(width) * scale;
    const float draw_h = static_cast<float>(height) * scale;
    return {
        bounds.x + (bounds.width - draw_w) * 0.5f,
        bounds.y + (bounds.height - draw_h) * 0.5f,
        draw_w,
        draw_h,
    };
}

Rectangle map_source_to_dest(Rectangle source, Rectangle image_dest, int width, int height) {
    const float scale_x = image_dest.width / static_cast<float>(width);
    const float scale_y = image_dest.height / static_cast<float>(height);
    return {
        image_dest.x + source.x * scale_x,
        image_dest.y + source.y * scale_y,
        source.width * scale_x,
        source.height * scale_y,
    };
}

Vector2 map_dest_to_source(Vector2 point, Rectangle image_dest, int width, int height) {
    return {
        (point.x - image_dest.x) * static_cast<float>(width) / image_dest.width,
        (point.y - image_dest.y) * static_cast<float>(height) / image_dest.height,
    };
}

float crop_size_for_zoom(int width, int height, float zoom) {
    return std::max(1.0f, static_cast<float>(std::min(width, height)) / std::max(kMinZoom, zoom));
}

Rectangle clamped_crop_rect(Rectangle crop, int width, int height) {
    crop.width = std::clamp(crop.width, 1.0f, static_cast<float>(std::max(1, width)));
    crop.height = std::clamp(crop.height, 1.0f, static_cast<float>(std::max(1, height)));
    crop.x = std::clamp(crop.x, 0.0f, static_cast<float>(width) - crop.width);
    crop.y = std::clamp(crop.y, 0.0f, static_cast<float>(height) - crop.height);
    return crop;
}

}  // namespace

state::~state() {
    unload();
}

bool state::open(const std::string& source_path, std::string& error_message) {
    unload();
    Image image = LoadImage(source_path.c_str());
    if (image.data == nullptr || image.width <= 0 || image.height <= 0) {
        error_message = "Failed to load image: " + source_path;
        if (image.data != nullptr) {
            UnloadImage(image);
        }
        return false;
    }

    texture_ = LoadTextureFromImage(image);
    image_width_ = image.width;
    image_height_ = image.height;
    UnloadImage(image);

    if (texture_.id == 0) {
        error_message = "Failed to prepare image preview.";
        return false;
    }

    SetTextureFilter(texture_, TEXTURE_FILTER_BILINEAR);
    source_path_ = source_path;
    crop_center_ = {static_cast<float>(image_width_) * 0.5f,
                    static_cast<float>(image_height_) * 0.5f};
    zoom_ = 1.0f;
    open_ = true;
    accepted_ = false;
    canceled_ = false;
    dragging_ = false;
    texture_loaded_ = true;
    return true;
}

void state::close() {
    open_ = false;
    dragging_ = false;
}

bool state::is_open() const {
    return open_;
}

void state::update() {
    if (!open_) {
        return;
    }

    ui::register_hit_region(kOverlayRect, ui::draw_layer::overlay);
    ui::register_hit_region(kDialogRect, ui::draw_layer::modal);

    if (IsKeyPressed(KEY_ESCAPE) || ui::is_clicked(kCancelRect, ui::draw_layer::modal)) {
        canceled_ = true;
        close();
        return;
    }
    if (ui::is_clicked(kApplyRect, ui::draw_layer::modal)) {
        accepted_ = true;
        close();
        return;
    }

    const Rectangle image_dest = fit_rect(kPreviewRect, image_width_, image_height_);
    const Rectangle crop_dest = map_source_to_dest(source_crop_rect(), image_dest, image_width_, image_height_);
    const Vector2 mouse = virtual_screen::get_virtual_mouse();

    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) &&
        CheckCollisionPointRec(mouse, crop_dest) &&
        !ui::is_blocked_by_higher_layer(crop_dest, ui::draw_layer::modal)) {
        dragging_ = true;
        drag_offset_ = {
            mouse.x - (crop_dest.x + crop_dest.width * 0.5f),
            mouse.y - (crop_dest.y + crop_dest.height * 0.5f),
        };
    }
    if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
        dragging_ = false;
    }
    if (dragging_ && IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
        const Vector2 source = map_dest_to_source(
            {mouse.x - drag_offset_.x, mouse.y - drag_offset_.y},
            image_dest,
            image_width_,
            image_height_);
        crop_center_ = source;
        clamp_center();
    }

    if (IsMouseButtonDown(MOUSE_BUTTON_LEFT) &&
        ui::is_hovered(kZoomRowRect, ui::draw_layer::modal)) {
        constexpr float kTrackLeftInset = 160.0f;
        constexpr float kTrackRightInset = 32.0f;
        const float track_x = kZoomRowRect.x + kTrackLeftInset;
        const float track_w = kZoomRowRect.width - kTrackLeftInset - kTrackRightInset;
        const float ratio = std::clamp((mouse.x - track_x) / track_w, 0.0f, 1.0f);
        set_zoom(kMinZoom + ratio * (kMaxZoom - kMinZoom));
    }
}

void state::draw() const {
    if (!open_) {
        return;
    }

    const auto& t = *g_theme;
    ui::draw_fullscreen_overlay(with_alpha(BLACK, 145));
    ui::draw_section(kDialogRect);
    ui::draw_text_in_rect("Crop Image", 28, kTitleRect, t.text, ui::text_align::left);

    ui::draw_rect_f(kPreviewRect, with_alpha(t.panel, 235));
    ui::draw_rect_lines(kPreviewRect, 1.5f, t.border_light);

    if (texture_loaded_ && texture_.id != 0) {
        const Rectangle image_dest = fit_rect(kPreviewRect, image_width_, image_height_);
        DrawTexturePro(texture_,
                       {0.0f, 0.0f, static_cast<float>(texture_.width), static_cast<float>(texture_.height)},
                       image_dest, {0.0f, 0.0f}, 0.0f, WHITE);

        const Rectangle crop_dest = map_source_to_dest(source_crop_rect(), image_dest, image_width_, image_height_);
        DrawRectangleRec({image_dest.x, image_dest.y, image_dest.width, crop_dest.y - image_dest.y},
                         with_alpha(BLACK, 105));
        DrawRectangleRec({image_dest.x, crop_dest.y + crop_dest.height, image_dest.width,
                          image_dest.y + image_dest.height - crop_dest.y - crop_dest.height},
                         with_alpha(BLACK, 105));
        DrawRectangleRec({image_dest.x, crop_dest.y, crop_dest.x - image_dest.x, crop_dest.height},
                         with_alpha(BLACK, 105));
        DrawRectangleRec({crop_dest.x + crop_dest.width, crop_dest.y,
                          image_dest.x + image_dest.width - crop_dest.x - crop_dest.width, crop_dest.height},
                         with_alpha(BLACK, 105));
        ui::draw_rect_lines(crop_dest, 3.0f, t.accent);
        ui::draw_rect_lines(ui::inset(crop_dest, -4.0f), 1.5f, with_alpha(t.bg, 210));
    }

    ui::draw_slider_relative(kZoomRowRect, "Zoom", TextFormat("%.1fx", zoom_),
                             (zoom_ - kMinZoom) / (kMaxZoom - kMinZoom),
                             160.0f, 32.0f, ui::draw_layer::modal,
                             18, 26.0f, 96.0f);
    ui::draw_button(kCancelRect, "CANCEL", 14);
    ui::draw_button_colored(kApplyRect, "APPLY", 14, t.row_active, t.row_hover, t.text);
}

bool state::consume_accept() {
    const bool accepted = accepted_;
    accepted_ = false;
    return accepted;
}

bool state::consume_cancel() {
    const bool canceled = canceled_;
    canceled_ = false;
    return canceled;
}

const std::string& state::source_path() const {
    return source_path_;
}

export_result state::export_png(const std::string& destination_path, export_options options) const {
    return export_square_png(source_path_, destination_path, source_crop_rect(), options);
}

void state::unload() {
    if (texture_loaded_) {
        UnloadTexture(texture_);
    }
    texture_ = {};
    texture_loaded_ = false;
    image_width_ = 0;
    image_height_ = 0;
    source_path_.clear();
}

Rectangle state::source_crop_rect() const {
    const float size = crop_size_for_zoom(image_width_, image_height_, zoom_);
    return clamped_crop_rect({crop_center_.x - size * 0.5f, crop_center_.y - size * 0.5f, size, size},
                             image_width_, image_height_);
}

void state::clamp_center() {
    const float half = crop_size_for_zoom(image_width_, image_height_, zoom_) * 0.5f;
    crop_center_.x = std::clamp(crop_center_.x, half, static_cast<float>(image_width_) - half);
    crop_center_.y = std::clamp(crop_center_.y, half, static_cast<float>(image_height_) - half);
}

void state::set_zoom(float zoom) {
    zoom_ = std::clamp(zoom, kMinZoom, kMaxZoom);
    clamp_center();
}

export_result export_square_png(const std::string& source_path,
                                const std::string& destination_path,
                                Rectangle source_crop,
                                export_options options) {
    Image image = LoadImage(source_path.c_str());
    if (image.data == nullptr || image.width <= 0 || image.height <= 0) {
        if (image.data != nullptr) {
            UnloadImage(image);
        }
        return {false, "Failed to load image: " + source_path};
    }

    source_crop = clamped_crop_rect(source_crop, image.width, image.height);
    ImageCrop(&image, source_crop);
    ImageResize(&image, options.output_size, options.output_size);

    const std::filesystem::path destination = path_utils::from_utf8(destination_path);
    std::error_code ec;
    std::filesystem::create_directories(destination.parent_path(), ec);

    const bool exported = ExportImage(image, destination_path.c_str());
    UnloadImage(image);
    if (!exported) {
        return {false, "Failed to write image: " + destination_path};
    }
    return {true, {}};
}

export_result export_center_square_png(const std::string& source_path,
                                       const std::string& destination_path,
                                       export_options options) {
    Image image = LoadImage(source_path.c_str());
    if (image.data == nullptr || image.width <= 0 || image.height <= 0) {
        if (image.data != nullptr) {
            UnloadImage(image);
        }
        return {false, "Failed to load image: " + source_path};
    }

    const float size = static_cast<float>(std::min(image.width, image.height));
    const Rectangle crop = {
        (static_cast<float>(image.width) - size) * 0.5f,
        (static_cast<float>(image.height) - size) * 0.5f,
        size,
        size,
    };
    UnloadImage(image);
    return export_square_png(source_path, destination_path, crop, options);
}

}  // namespace square_image_picker
