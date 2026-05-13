#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace chart_file_storage {

bool write_validated_raw_chart_file(const std::filesystem::path& path,
                                    const std::vector<unsigned char>& bytes,
                                    std::string& error_message);

}  // namespace chart_file_storage
