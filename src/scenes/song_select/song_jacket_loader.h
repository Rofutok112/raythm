#pragma once

#include <future>
#include <optional>
#include <string>
#include <vector>

#include "raylib.h"
#include "song_select/selection_key.h"
#include "song_select/song_select_state.h"

namespace song_select {

class jacket_loader {
public:
    enum class load_status {
        idle,
        loading,
        ready,
        failed,
    };

    struct snapshot {
        load_status status = load_status::idle;
        std::optional<selection_key> key;
    };

    jacket_loader() = default;
    jacket_loader(const jacket_loader&) = delete;
    jacket_loader& operator=(const jacket_loader&) = delete;
    ~jacket_loader();

    void request(const selection_key& key, const song_entry* song);
    void request(const song_entry* song);
    void poll();
    void reset();

    [[nodiscard]] load_status status() const;
    [[nodiscard]] snapshot current() const;
    [[nodiscard]] bool loaded() const;
    [[nodiscard]] const Texture2D& texture() const;

private:
    struct pending_texture {
        selection_key key;
        std::vector<unsigned char> bytes;
        std::string file_type;
    };

    void unload();
    static pending_texture load_bytes(selection_key key, song_data song);
    static Texture2D load_texture(const pending_texture& pending);

    selection_key target_key_;
    std::future<pending_texture> future_;
    Texture2D texture_{};
    load_status status_ = load_status::idle;
};

}  // namespace song_select
