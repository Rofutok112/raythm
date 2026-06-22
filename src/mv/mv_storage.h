#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include "data_models.h"
#include "mv/composition/mv_composition.h"
#include "mv/mv_managed_storage.h"

namespace mv {

struct mv_metadata {
    std::string mv_id;
    std::string song_id;
    std::string name;
    std::string author;
    std::string description;
    int format_version = 2;
    std::string composition_file = "composition.rmvcomp";
};

struct mv_package {
    mv_metadata meta;
    std::string directory;
    storage_policy storage = storage_policy::plain_workspace;
    std::optional<managed_storage::package_manifest> managed_manifest;
    double song_duration_ms = 0.0;
};

struct edit_access_result {
    bool editable = true;
    std::string reason;
};

std::vector<mv_package> load_all_packages();
std::optional<mv_package> find_first_package_for_song(const std::string& song_id);
mv_package make_default_package_for_song(const song_meta& song);
edit_access_result can_edit_package(const mv_package& package);
std::filesystem::path composition_path(const mv_package& package);
bool write_mv_json(const mv_metadata& meta, const std::string& directory);
bool save_metadata(const mv_package& package, std::vector<std::string>* errors = nullptr);
composition::mv_composition make_default_composition_for_song(const mv_package& package);
std::optional<composition::mv_composition> load_composition(const mv_package& package,
                                                            std::vector<std::string>* errors = nullptr);
bool save_composition(const mv_package& package, const composition::mv_composition& composition);
bool import_composition(const mv_package& package, const std::string& source_path_utf8,
                        std::vector<std::string>* errors = nullptr);
bool export_composition(const mv_package& package, const std::string& destination_path_utf8);
bool import_package(const std::string& source_path_utf8,
                    const std::string& target_song_id = {},
                    std::vector<std::string>* errors = nullptr);
bool export_package(const mv_package& package,
                    const std::string& destination_path_utf8,
                    std::vector<std::string>* errors = nullptr);
bool ensure_composition_package(const mv_package& package);
std::optional<composition::asset_ref> import_image_asset(const mv_package& package,
                                                         const std::string& source_path_utf8,
                                                         std::vector<std::string>* errors = nullptr);
std::filesystem::path resolve_asset_path(const mv_package& package, const composition::asset_ref& asset);
std::optional<std::vector<unsigned char>> read_asset_bytes(const mv_package& package,
                                                           const composition::asset_ref& asset,
                                                           std::vector<std::string>* errors = nullptr);

}  // namespace mv
