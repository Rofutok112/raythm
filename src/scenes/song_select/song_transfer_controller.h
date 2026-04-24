#pragma once

#include <future>
#include <optional>
#include <string>
#include <vector>

#include "song_select/song_import_export_service.h"

namespace song_select::transfer {

class controller {
public:
    void on_exit();

    [[nodiscard]] bool prepare_active() const { return prepare_active_; }
    [[nodiscard]] bool transfer_active() const { return transfer_active_; }
    [[nodiscard]] bool busy() const { return prepare_active_ || transfer_active_; }
    [[nodiscard]] const std::string& busy_label() const { return busy_label_; }

    void start_song_import_prepare(const state& catalog_state, std::string source_path);
    void start_song_import_prepare(const state& catalog_state, std::vector<std::string> source_paths);
    void start_song_export(song_export_request request);
    void start_song_import(song_import_request request);
    void start_song_imports(std::vector<song_import_request> requests);

    std::optional<song_import_prepare_batch_result> poll_song_import_prepare();
    std::optional<transfer_result> poll_background_transfer();

    void set_pending_song_import_request(song_import_request request);
    void set_pending_song_import_requests(std::vector<song_import_request> requests);
    [[nodiscard]] const std::vector<song_import_request>& pending_song_import_requests() const {
        return pending_song_import_requests_;
    }
    void clear_pending_song_import_request(bool cleanup_request);

    void set_pending_chart_import_request(chart_import_request request);
    void set_pending_chart_import_requests(std::vector<chart_import_request> requests);
    [[nodiscard]] const std::vector<chart_import_request>& pending_chart_import_requests() const {
        return pending_chart_import_requests_;
    }
    void clear_pending_chart_import_request();

private:
    std::future<transfer_result> background_transfer_;
    std::future<song_import_prepare_batch_result> background_song_import_prepare_;
    bool transfer_active_ = false;
    bool prepare_active_ = false;
    std::string busy_label_;
    std::vector<song_import_request> pending_song_import_requests_;
    std::vector<chart_import_request> pending_chart_import_requests_;
};

}  // namespace song_select::transfer
