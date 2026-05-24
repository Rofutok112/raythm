#include "shared/avatar_texture_cache.h"

#include <algorithm>
#include <chrono>
#include <future>
#include <string>
#include <string_view>
#include <utility>

#include "network/http_client.h"
#include "network/server_environment.h"
#include "theme.h"
#include "ui_draw.h"

namespace avatar_texture_cache {
namespace {

bool starts_with(std::string_view value, std::string_view prefix) {
    return value.size() >= prefix.size() && value.substr(0, prefix.size()) == prefix;
}

Image load_image_from_bytes(std::vector<unsigned char>& bytes) {
    if (bytes.empty()) {
        return {};
    }

    Image image = LoadImageFromMemory(".png", bytes.data(), static_cast<int>(bytes.size()));
    if (image.data == nullptr) {
        image = LoadImageFromMemory(".jpg", bytes.data(), static_cast<int>(bytes.size()));
    }
    return image;
}

}  // namespace

cache::~cache() {
    clear();
}

const Texture2D* cache::get(const std::string& avatar_url, const std::string& base_url) {
    const std::string resolved_url = resolve_url(avatar_url, base_url);
    if (resolved_url.empty()) {
        return nullptr;
    }

    entry& item = entries_[resolved_url];
    if (item.loaded && item.texture.id != 0) {
        return &item.texture;
    }
    if (item.missing) {
        return nullptr;
    }
    if (!item.requested) {
        item.requested = true;
        item.future = std::async(std::launch::async, [resolved_url]() {
            const network::http::response response = network::http::send_request(
                "GET",
                resolved_url,
                {
                    {"Accept", "image/png,image/jpeg,*/*"},
                    {"User-Agent", "raythm/0.1"},
                });
            pending_image image;
            if (response.error_message.empty() && response.status_code >= 200 && response.status_code < 300) {
                image.bytes.assign(response.body.begin(), response.body.end());
            }
            return image;
        });
    }
    return nullptr;
}

void cache::poll() {
    for (auto& [_, item] : entries_) {
        if (!item.requested || item.loaded || item.missing ||
            item.future.wait_for(std::chrono::milliseconds(0)) != std::future_status::ready) {
            continue;
        }

        pending_image pending = item.future.get();
        Image image = load_image_from_bytes(pending.bytes);
        if (image.data == nullptr || image.width <= 0 || image.height <= 0) {
            if (image.data != nullptr) {
                UnloadImage(image);
            }
            item.missing = true;
            continue;
        }

        item.texture = LoadTextureFromImage(image);
        UnloadImage(image);
        if (item.texture.id == 0) {
            item.missing = true;
            continue;
        }

        SetTextureFilter(item.texture, TEXTURE_FILTER_BILINEAR);
        item.loaded = true;
    }
}

void cache::clear() {
    for (auto& [_, item] : entries_) {
        if (item.loaded && item.texture.id != 0) {
            UnloadTexture(item.texture);
        }
        item.texture = {};
        item.loaded = false;
    }
    entries_.clear();
}

cache& shared() {
    static cache instance;
    return instance;
}

std::string resolve_url(const std::string& avatar_url, const std::string& base_url) {
    if (avatar_url.empty()) {
        return {};
    }
    if (starts_with(avatar_url, "http://") || starts_with(avatar_url, "https://")) {
        return avatar_url;
    }
    const std::string normalized_base = server_environment::normalize_url(base_url);
    if (normalized_base.empty()) {
        return {};
    }
    if (avatar_url.front() == '/') {
        return normalized_base + avatar_url;
    }
    return normalized_base + "/" + avatar_url;
}

void draw_avatar(Rectangle rect,
                 const std::string& avatar_url,
                 const std::string& fallback_label,
                 Color background,
                 Color text_color,
                 int font_size,
                 const std::string& base_url) {
    shared().poll();
    if (const Texture2D* texture = shared().get(avatar_url, base_url); texture != nullptr) {
        DrawTexturePro(*texture,
                       {0.0f, 0.0f, static_cast<float>(texture->width), static_cast<float>(texture->height)},
                       rect,
                       {0.0f, 0.0f},
                       0.0f,
                       WHITE);
        ui::draw_rect_lines(rect, 1.5f, with_alpha(g_theme->border_light, 220));
        return;
    }

    ui::draw_rect_f(rect, background);
    ui::draw_text_in_rect(fallback_label.c_str(), font_size, rect, text_color, ui::text_align::center);
}

}  // namespace avatar_texture_cache
