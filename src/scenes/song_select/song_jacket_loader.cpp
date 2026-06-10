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
    request(song_media_key_for_song_id(song->song.meta.song_id), song);
}

void jacket_loader::request(const selection_key& key, const song_entry* song) {
    if (song == nullptr) {
        reset();
        return;
    }

    const selection_key target_key = song_media_key_for(key);
    if (target_key_ == target_key &&
        (status_ == load_status::loading || status_ == load_status::ready || status_ == load_status::failed)) {
        return;
    }

    unload();
    target_key_ = target_key;
    if (song->song.meta.jacket_file.empty()) {
        status_ = load_status::failed;
        return;
    }

    std::promise<pending_texture> promise;
    future_ = promise.get_future();
    const song_data song_copy = song->song;
    std::thread([promise = std::move(promise), target_key, song_copy]() mutable {
        try {
            promise.set_value(load_bytes(target_key, song_copy));
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

    if (pending.key != target_key_) {
        status_ = pending.key.song_id.empty() ? load_status::failed : load_status::idle;
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
    target_key_ = {};
    future_ = {};
    unload();
    status_ = load_status::idle;
}

jacket_loader::load_status jacket_loader::status() const {
    return status_;
}

jacket_loader::snapshot jacket_loader::current() const {
    snapshot result;
    result.status = status_;
    if (status_ != load_status::idle && !target_key_.song_id.empty()) {
        result.key = target_key_;
    }
    return result;
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

jacket_loader::pending_texture jacket_loader::load_bytes(selection_key key, song_data song) {
    pending_texture result;
    result.key = song_media_key_for(key);
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
