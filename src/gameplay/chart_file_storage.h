#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace chart_file_storage {

bool write_validated_raw_chart_file(const std::filesystem::path& path,
                                    const std::vector<unsigned char>& bytes,
                                    std::string& error_message);
bool write_validated_chart_file_with_local_id(const std::filesystem::path& path,
                                              const std::vector<unsigned char>& bytes,
                                              const std::string& local_chart_id,
                                              std::string& error_message);

}  // namespace chart_file_storage
