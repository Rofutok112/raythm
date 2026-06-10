#pragma once

#include <future>
#include <string>
#include <vector>

#include "raylib.h"
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

    jacket_loader() = default;
    jacket_loader(const jacket_loader&) = delete;
    jacket_loader& operator=(const jacket_loader&) = delete;
    ~jacket_loader();

    void request(const song_entry* song);
    void poll();
    void reset();

    [[nodiscard]] load_status status() const;
    [[nodiscard]] bool loaded() const;
    [[nodiscard]] const Texture2D& texture() const;

private:
    struct pending_texture {
        std::string song_id;
        std::vector<unsigned char> bytes;
        std::string file_type;
    };

    void unload();
    static pending_texture load_bytes(song_data song);
    static Texture2D load_texture(const pending_texture& pending);

    std::string target_song_id_;
    std::future<pending_texture> future_;
    Texture2D texture_{};
    load_status status_ = load_status::idle;
};

}  // namespace song_select
