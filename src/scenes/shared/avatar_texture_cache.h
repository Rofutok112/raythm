#pragma once

#include <future>
#include <string>
#include <unordered_map>
#include <vector>

#include "raylib.h"

namespace avatar_texture_cache {

class cache {
public:
    cache() = default;
    ~cache();

    cache(const cache&) = delete;
    cache& operator=(const cache&) = delete;

    const Texture2D* get(const std::string& avatar_url, const std::string& base_url = {});
    void poll();
    void clear();

private:
    struct pending_image {
        std::vector<unsigned char> bytes;
    };

    struct entry {
        Texture2D texture{};
        std::future<pending_image> future;
        bool requested = false;
        bool loaded = false;
        bool missing = false;
    };

    std::unordered_map<std::string, entry> entries_;
};

cache& shared();

std::string resolve_url(const std::string& avatar_url, const std::string& base_url);
void draw_avatar(Rectangle rect,
                 const std::string& avatar_url,
                 const std::string& fallback_label,
                 Color background,
                 Color text_color,
                 int font_size,
                 const std::string& base_url = {});

}  // namespace avatar_texture_cache
