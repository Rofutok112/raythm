#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include "data_models.h"

namespace mv {

struct mv_metadata {
    std::string mv_id;
    std::string song_id;
    std::string name;
    std::string author;
    std::string script_file = "script.rmv";
};

struct mv_package {
    mv_metadata meta;
    std::string directory;
};

std::vector<mv_package> load_all_packages();
std::optional<mv_package> find_first_package_for_song(const std::string& song_id);
mv_package make_default_package_for_song(const song_meta& song);
std::filesystem::path script_path(const mv_package& package);
bool write_mv_json(const mv_metadata& meta, const std::string& directory);
std::string load_script(const mv_package& package);
bool save_script(const mv_package& package, const std::string& script);

}  // namespace mv
