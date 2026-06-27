#include "shared/square_image_picker.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <string>

#include "file_io.h"
#include "path_utils.h"
#include "raylib_file_io.h"
#include "scene_common.h"
#include "theme.h"
#include "ui_draw.h"
#include "ui_hit.h"
#include "ui_layout.h"
#include "virtual_screen.h"

namespace square_image_picker {
namespace {

constexpr Rectangle kOverlayRect = {0.0f, 0.0f, static_cast<float>(kScreenWidth), static_cast<float>(kScreenHeight)};
constexpr Rectangle kDialogRect = {455.0f, 86.0f, 1010.0f, 908.0f};
constexpr float kTitleWidth = 540.0f;
constexpr float kTitleHeight = 40.0f;
constexpr Vector2 kTitleOffset = {46.0f, 40.0f};
constexpr float kPreviewWidth = 790.0f;
constexpr float kPreviewHeight = 630.0f;
constexpr Vector2 kPreviewOffset = {110.0f, 102.0f};
constexpr float kZoomTopGap = 24.0f;
constexpr float kZoomRowHeight = 54.0f;
constexpr float kZoomTrackLeftInset = 160.0f;
constexpr float kZoomTrackRightInset = 32.0f;
constexpr float kZoomTrackTopOffset = 26.0f;
constexpr float kZoomLabelWidth = 96.0f;
constexpr float kActionButtonHeight = 48.0f;
constexpr float kActionButtonGap = 16.0f;
constexpr float kCancelButtonWidth = 132.0f;
constexpr float kApplyButtonWidth = 140.0f;
constexpr float kActionBottomInset = 28.0f;
constexpr float kMinZoom = 1.0f;
constexpr float kMaxZoom = 4.0f;

enum class picker_action {
    none,
    cancel,
    apply,
};

struct picker_layout {
    Rectangle overlay;
    Rectangle dialog;
    Rectangle title;
    Rectangle preview;
    Rectangle zoom_row;
    Rectangle cancel_button;
    Rectangle apply_button;
};

constexpr picker_layout picker_layout_for(Rectangle dialog) {
    const Rectangle title = ui::place(dialog, kTitleWidth, kTitleHeight,
                                      ui::anchor::top_left, ui::anchor::top_left,
                                      kTitleOffset);
    const Rectangle preview = ui::place(dialog, kPreviewWidth, kPreviewHeight,
                                        ui::anchor::top_left, ui::anchor::top_left,
                                        kPreviewOffset);
    const Rectangle zoom_row = {preview.x, preview.y + preview.height + kZoomTopGap,
                                preview.width, kZoomRowHeight};
    const float actions_width = kCancelButtonWidth + kActionButtonGap + kApplyButtonWidth;
    const Rectangle actions = {
        preview.x + preview.width - actions_width,
        dialog.y + dialog.height - kActionBottomInset - kActionButtonHeight,
        actions_width,
        kActionButtonHeight,
    };
    const ui::rect_pair action_buttons =
        ui::split_trailing(actions, kApplyButtonWidth, kActionButtonGap);
    return {
        kOverlayRect,
        dialog,
        title,
        preview,
        zoom_row,
        action_buttons.first,
        action_buttons.second,
    };
}

constexpr picker_layout kLayout = picker_layout_for(kDialogRect);

struct picker_action_button {
    picker_action action;
    Rectangle rect;
    const char* label;
    bool primary = false;
};

constexpr picker_action_button kActionButtons[] = {
    {picker_action::cancel, kLayout.cancel_button, "CANCEL", false},
    {picker_action::apply, kLayout.apply_button, "APPLY", true},
};

constexpr Rectangle zoom_track_rect(Rectangle zoom_row) {
    return {
        zoom_row.x + kZoomTrackLeftInset,
        zoom_row.y,
        zoom_row.width - kZoomTrackLeftInset - kZoomTrackRightInset,
        zoom_row.height,
    };
}

float zoom_for_mouse_x(Rectangle zoom_row, float mouse_x) {
    const Rectangle track = zoom_track_rect(zoom_row);
    const float ratio = std::clamp((mouse_x - track.x) / track.width, 0.0f, 1.0f);
    return kMinZoom + ratio * (kMaxZoom - kMinZoom);
}

picker_action clicked_picker_action() {
    for (const picker_action_button& button : kActionButtons) {
        if (ui::is_clicked(button.rect, ui::draw_layer::modal)) {
            return button.action;
        }
    }
    return picker_action::none;
}

ui::slider_options zoom_slider_options() {
    return {
        .layer = ui::draw_layer::modal,
        .font_size = 18,
        .track_top_offset = kZoomTrackTopOffset,
        .label_width = kZoomLabelWidth,
    };
}

bool write_png_image_file(const Image& image, const std::filesystem::path& destination) {
    int output_size = 0;
    unsigned char* output = ExportImageToMemory(image, ".png", &output_size);
    if (output == nullptr || output_size <= 0) {
        if (output != nullptr) {
            MemFree(output);
        }
        return false;
    }

    const bool ok = file_io::write_binary_file(destination, output, output_size);
    MemFree(output);
    return ok;
}

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

void draw_picker_action_buttons(const ui_theme& theme) {
    for (const picker_action_button& button : kActionButtons) {
        if (button.primary) {
            ui::button(button.rect, button.label, {
                .layer = ui::draw_layer::modal,
                .font_size = 14,
                .border_width = 2.0f,
                .bg = theme.row_active,
                .bg_hover = theme.row_hover,
                .text_color = theme.text,
                .custom_colors = true,
            });
        } else {
            ui::button(button.rect, button.label, {
                .layer = ui::draw_layer::modal,
                .font_size = 14,
            });
        }
    }
}

}  // namespace

state::~state() {
    unload();
}

bool state::open(const std::string& source_path, std::string& error_message) {
    unload();
    Image image = raylib_file_io::load_image_utf8(source_path);
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

    ui::register_hit_region(kLayout.overlay, ui::draw_layer::overlay);
    ui::register_hit_region(kLayout.dialog, ui::draw_layer::modal);

    const picker_action action = clicked_picker_action();
    if (IsKeyPressed(KEY_ESCAPE) || action == picker_action::cancel) {
        canceled_ = true;
        close();
        return;
    }
    if (action == picker_action::apply) {
        accepted_ = true;
        close();
        return;
    }

    const Rectangle image_dest = fit_rect(kLayout.preview, image_width_, image_height_);
    const Rectangle crop_dest = map_source_to_dest(source_crop_rect(), image_dest, image_width_, image_height_);
    const Vector2 mouse = virtual_screen::get_virtual_mouse();

    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) &&
        ui::contains_point(crop_dest, mouse) &&
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
        ui::is_hovered(kLayout.zoom_row, ui::draw_layer::modal)) {
        set_zoom(zoom_for_mouse_x(kLayout.zoom_row, mouse.x));
    }
}

void state::draw() const {
    if (!open_) {
        return;
    }

    const auto& t = *g_theme;
    ui::draw_fullscreen_overlay(with_alpha(BLACK, 145));
    ui::section(kLayout.dialog);
    ui::draw_text_in_rect("Crop Image", 28, kLayout.title, t.text, ui::text_align::left);

    ui::surface(kLayout.preview, with_alpha(t.panel, 235), t.border_light, 1.5f);

    if (texture_loaded_ && texture_.id != 0) {
        const Rectangle image_dest = fit_rect(kLayout.preview, image_width_, image_height_);
        ui::draw_texture(texture_, image_dest);

        const Rectangle crop_dest = map_source_to_dest(source_crop_rect(), image_dest, image_width_, image_height_);
        ui::dim_outside_rect(image_dest, crop_dest, with_alpha(BLACK, 105));
        ui::frame(crop_dest, t.accent, 3.0f);
        ui::frame(ui::inset(crop_dest, -4.0f), with_alpha(t.bg, 210), 1.5f);
    }

    ui::slider_relative(kLayout.zoom_row, "Zoom", TextFormat("%.1fx", zoom_),
                        (zoom_ - kMinZoom) / (kMaxZoom - kMinZoom),
                        kZoomTrackLeftInset, kZoomTrackRightInset, zoom_slider_options());
    draw_picker_action_buttons(t);
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
    Image image = raylib_file_io::load_image_utf8(source_path);
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

    const bool exported = write_png_image_file(image, destination);
    UnloadImage(image);
    if (!exported) {
        return {false, "Failed to write image: " + destination_path};
    }
    return {true, {}};
}

export_result export_center_square_png(const std::string& source_path,
                                       const std::string& destination_path,
                                       export_options options) {
    Image image = raylib_file_io::load_image_utf8(source_path);
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
