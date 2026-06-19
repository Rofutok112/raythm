#pragma once

#include <string>

namespace local_catalog_signature {

constexpr const char* kStatusSchema = "managed-content-v4-catalog-files";

bool is_compatible_status_schema(const std::string& schema);

std::string current();

}  // namespace local_catalog_signature
