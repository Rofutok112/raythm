#include "song_select/song_jacket_loader.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <thread>
#include <utility>

#include "managed_content_storage.h"
#include "path_utils.h"

namespace song_select {

jacket_loader::~jacket_loader() {
    reset();
}

void jacket_loader::request(const song_entry* song) {
    if (song == nullptr) {
        reset();
        return;
    }

    const std::string song_id = song->song.meta.song_id;
    if (target_song_id_ == song_id &&
        (status_ == load_status::loading || status_ == load_status::ready || status_ == load_status::failed)) {
        return;
    }

    unload();
    target_song_id_ = song_id;
    if (song->song.meta.jacket_file.empty()) {
        status_ = load_status::failed;
        return;
    }

    std::promise<pending_texture> promise;
    future_ = promise.get_future();
    const song_data song_copy = song->song;
    std::thread([promise = std::move(promise), song_copy]() mutable {
        try {
            promise.set_value(load_bytes(song_copy));
        } catch (...) {
            promise.set_exception(std::current_exception());
        }
    }).detach();
    status_ = load_status::loading;
}

void jacket_loader::poll() {
    if (status_ != load_status::loading || !future_.valid()) {
        return;
    }
    if (future_.wait_for(std::chrono::milliseconds(0)) != std::future_status::ready) {
        return;
    }

    pending_texture pending;
    try {
        pending = future_.get();
    } catch (...) {
        pending = {};
    }

    if (pending.song_id != target_song_id_) {
        return;
    }

    texture_ = load_texture(pending);
    if (texture_.id == 0) {
        status_ = load_status::failed;
        return;
    }

    SetTextureFilter(texture_, TEXTURE_FILTER_BILINEAR);
    status_ = load_status::ready;
}

void jacket_loader::reset() {
    target_song_id_.clear();
    future_ = {};
    unload();
    status_ = load_status::idle;
}

jacket_loader::load_status jacket_loader::status() const {
    return status_;
}

bool jacket_loader::loaded() const {
    return status_ == load_status::ready && texture_.id != 0;
}

const Texture2D& jacket_loader::texture() const {
    return texture_;
}

void jacket_loader::unload() {
    if (texture_.id != 0) {
        UnloadTexture(texture_);
        texture_ = {};
    }
}

jacket_loader::pending_texture jacket_loader::load_bytes(song_data song) {
    pending_texture result;
    result.song_id = song.meta.song_id;
    if (song.meta.jacket_file.empty()) {
        return result;
    }

    const std::filesystem::path jacket_path = path_utils::join_utf8(song.directory, song.meta.jacket_file);
    const managed_content_storage::managed_file_read_result managed =
        managed_content_storage::read_managed_file(jacket_path);
    if (managed.managed) {
        if (managed.success) {
            result.bytes = managed.bytes;
            result.file_type = jacket_path.extension().string();
        }
        return result;
    }

    std::error_code ec;
    if (!std::filesystem::exists(jacket_path, ec) || !std::filesystem::is_regular_file(jacket_path, ec)) {
        return result;
    }

    std::ifstream input(jacket_path, std::ios::binary);
    if (!input.is_open()) {
        return result;
    }

    input.seekg(0, std::ios::end);
    const std::streamoff size = input.tellg();
    input.seekg(0, std::ios::beg);
    if (size <= 0) {
        return result;
    }

    result.bytes.resize(static_cast<size_t>(size));
    input.read(reinterpret_cast<char*>(result.bytes.data()), size);
    if (!input.good() && !input.eof()) {
        result.bytes.clear();
        result.file_type.clear();
        return result;
    }

    result.file_type = jacket_path.extension().string();
    return result;
}

Texture2D jacket_loader::load_texture(const pending_texture& pending) {
    if (pending.bytes.empty() || pending.file_type.empty()) {
        return {};
    }

    Image image = LoadImageFromMemory(pending.file_type.c_str(),
                                      pending.bytes.data(),
                                      static_cast<int>(pending.bytes.size()));
    if (image.data == nullptr) {
        return {};
    }

    Texture2D texture = LoadTextureFromImage(image);
    UnloadImage(image);
    return texture;
}

}  // namespace song_select
