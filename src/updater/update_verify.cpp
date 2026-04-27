#include "updater/update_verify.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <fstream>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>

namespace {

struct sha256_state {
    std::array<std::uint32_t, 8> hash = {
        0x6a09e667u, 0xbb67ae85u, 0x3c6ef372u, 0xa54ff53au,
        0x510e527fu, 0x9b05688cu, 0x1f83d9abu, 0x5be0cd19u,
    };
    std::array<std::uint8_t, 64> buffer{};
    std::uint64_t bit_length = 0;
    std::size_t buffer_size = 0;
};

constexpr std::array<std::uint32_t, 64> kSha256Constants = {
    0x428a2f98u, 0x71374491u, 0xb5c0fbcfu, 0xe9b5dba5u, 0x3956c25bu, 0x59f111f1u, 0x923f82a4u, 0xab1c5ed5u,
    0xd807aa98u, 0x12835b01u, 0x243185beu, 0x550c7dc3u, 0x72be5d74u, 0x80deb1feu, 0x9bdc06a7u, 0xc19bf174u,
    0xe49b69c1u, 0xefbe4786u, 0x0fc19dc6u, 0x240ca1ccu, 0x2de92c6fu, 0x4a7484aau, 0x5cb0a9dcu, 0x76f988dau,
    0x983e5152u, 0xa831c66du, 0xb00327c8u, 0xbf597fc7u, 0xc6e00bf3u, 0xd5a79147u, 0x06ca6351u, 0x14292967u,
    0x27b70a85u, 0x2e1b2138u, 0x4d2c6dfcu, 0x53380d13u, 0x650a7354u, 0x766a0abbu, 0x81c2c92eu, 0x92722c85u,
    0xa2bfe8a1u, 0xa81a664bu, 0xc24b8b70u, 0xc76c51a3u, 0xd192e819u, 0xd6990624u, 0xf40e3585u, 0x106aa070u,
    0x19a4c116u, 0x1e376c08u, 0x2748774cu, 0x34b0bcb5u, 0x391c0cb3u, 0x4ed8aa4au, 0x5b9cca4fu, 0x682e6ff3u,
    0x748f82eeu, 0x78a5636fu, 0x84c87814u, 0x8cc70208u, 0x90befffau, 0xa4506cebu, 0xbef9a3f7u, 0xc67178f2u,
};

std::string to_lower_hex(const unsigned char* bytes, size_t length);

constexpr std::uint32_t rotr(std::uint32_t value, std::uint32_t amount) {
    return (value >> amount) | (value << (32 - amount));
}

void sha256_transform(sha256_state& state, const std::uint8_t* block) {
    std::uint32_t words[64];
    for (int index = 0; index < 16; ++index) {
        const int offset = index * 4;
        words[index] = (static_cast<std::uint32_t>(block[offset]) << 24) |
                       (static_cast<std::uint32_t>(block[offset + 1]) << 16) |
                       (static_cast<std::uint32_t>(block[offset + 2]) << 8) |
                       static_cast<std::uint32_t>(block[offset + 3]);
    }

    for (int index = 16; index < 64; ++index) {
        const std::uint32_t s0 = rotr(words[index - 15], 7) ^ rotr(words[index - 15], 18) ^ (words[index - 15] >> 3);
        const std::uint32_t s1 = rotr(words[index - 2], 17) ^ rotr(words[index - 2], 19) ^ (words[index - 2] >> 10);
        words[index] = words[index - 16] + s0 + words[index - 7] + s1;
    }

    std::uint32_t a = state.hash[0];
    std::uint32_t b = state.hash[1];
    std::uint32_t c = state.hash[2];
    std::uint32_t d = state.hash[3];
    std::uint32_t e = state.hash[4];
    std::uint32_t f = state.hash[5];
    std::uint32_t g = state.hash[6];
    std::uint32_t h = state.hash[7];

    for (int index = 0; index < 64; ++index) {
        const std::uint32_t s1 = rotr(e, 6) ^ rotr(e, 11) ^ rotr(e, 25);
        const std::uint32_t choice = (e & f) ^ (~e & g);
        const std::uint32_t temp1 = h + s1 + choice + kSha256Constants[index] + words[index];
        const std::uint32_t s0 = rotr(a, 2) ^ rotr(a, 13) ^ rotr(a, 22);
        const std::uint32_t majority = (a & b) ^ (a & c) ^ (b & c);
        const std::uint32_t temp2 = s0 + majority;

        h = g;
        g = f;
        f = e;
        e = d + temp1;
        d = c;
        c = b;
        b = a;
        a = temp1 + temp2;
    }

    state.hash[0] += a;
    state.hash[1] += b;
    state.hash[2] += c;
    state.hash[3] += d;
    state.hash[4] += e;
    state.hash[5] += f;
    state.hash[6] += g;
    state.hash[7] += h;
}

void sha256_update(sha256_state& state, const std::uint8_t* data, std::size_t length) {
    for (std::size_t index = 0; index < length; ++index) {
        state.buffer[state.buffer_size++] = data[index];
        if (state.buffer_size == state.buffer.size()) {
            sha256_transform(state, state.buffer.data());
            state.bit_length += 512;
            state.buffer_size = 0;
        }
    }
}

std::string sha256_finish(sha256_state& state) {
    state.bit_length += static_cast<std::uint64_t>(state.buffer_size) * 8;
    state.buffer[state.buffer_size++] = 0x80;

    if (state.buffer_size > 56) {
        while (state.buffer_size < 64) {
            state.buffer[state.buffer_size++] = 0x00;
        }
        sha256_transform(state, state.buffer.data());
        state.buffer_size = 0;
    }

    while (state.buffer_size < 56) {
        state.buffer[state.buffer_size++] = 0x00;
    }

    for (int shift = 56; shift >= 0; shift -= 8) {
        state.buffer[state.buffer_size++] = static_cast<std::uint8_t>((state.bit_length >> shift) & 0xffu);
    }
    sha256_transform(state, state.buffer.data());

    std::array<std::uint8_t, 32> digest{};
    for (std::size_t index = 0; index < state.hash.size(); ++index) {
        digest[index * 4] = static_cast<std::uint8_t>((state.hash[index] >> 24) & 0xffu);
        digest[index * 4 + 1] = static_cast<std::uint8_t>((state.hash[index] >> 16) & 0xffu);
        digest[index * 4 + 2] = static_cast<std::uint8_t>((state.hash[index] >> 8) & 0xffu);
        digest[index * 4 + 3] = static_cast<std::uint8_t>(state.hash[index] & 0xffu);
    }
    return to_lower_hex(digest.data(), digest.size());
}

std::string trim(std::string value) {
    const auto first = std::find_if_not(value.begin(), value.end(), [](unsigned char ch) { return std::isspace(ch); });
    const auto last =
        std::find_if_not(value.rbegin(), value.rend(), [](unsigned char ch) { return std::isspace(ch); }).base();
    if (first >= last) {
        return {};
    }
    return std::string(first, last);
}

std::string to_lower_hex(const unsigned char* bytes, size_t length) {
    static constexpr char kHexDigits[] = "0123456789abcdef";
    std::string result;
    result.reserve(length * 2);
    for (size_t index = 0; index < length; ++index) {
        result.push_back(kHexDigits[(bytes[index] >> 4) & 0x0F]);
        result.push_back(kHexDigits[bytes[index] & 0x0F]);
    }
    return result;
}

std::string read_file_text(const std::filesystem::path& path) {
    std::ifstream input(path);
    if (!input.is_open()) {
        return {};
    }

    std::ostringstream buffer;
    buffer << input.rdbuf();
    return buffer.str();
}

}  // namespace

namespace updater {

std::optional<std::string> compute_sha256_hex(const std::filesystem::path& file_path) {
    std::ifstream input(file_path, std::ios::binary);
    if (!input.is_open()) {
        return std::nullopt;
    }

    sha256_state state;
    std::array<char, 16 * 1024> buffer{};
    while (input.good()) {
        input.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
        const std::streamsize bytes_read = input.gcount();
        if (bytes_read > 0) {
            sha256_update(state, reinterpret_cast<const std::uint8_t*>(buffer.data()), static_cast<std::size_t>(bytes_read));
        }
    }
    return sha256_finish(state);
}

std::string compute_sha256_hex(std::string_view content) {
    sha256_state state;
    sha256_update(state,
                  reinterpret_cast<const std::uint8_t*>(content.data()),
                  content.size());
    return sha256_finish(state);
}

std::optional<std::string> parse_sha256sums_for_file(const std::string& checksums_content, const std::string& file_name) {
    std::istringstream stream(checksums_content);
    std::string line;
    while (std::getline(stream, line)) {
        const std::string trimmed = trim(line);
        if (trimmed.empty()) {
            continue;
        }

        const size_t split_pos = trimmed.find_first_of(" \t");
        if (split_pos == std::string::npos) {
            continue;
        }

        std::string hash_text = trim(trimmed.substr(0, split_pos));
        std::string listed_file = trim(trimmed.substr(split_pos));
        while (!listed_file.empty() && (listed_file.front() == '*' || listed_file.front() == ' ' || listed_file.front() == '\t')) {
            listed_file.erase(listed_file.begin());
        }

        std::transform(hash_text.begin(), hash_text.end(), hash_text.begin(),
                       [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
        if (listed_file == file_name) {
            return hash_text;
        }
    }

    return std::nullopt;
}

bool verify_sha256_checksum(const std::filesystem::path& file_path, const std::filesystem::path& checksums_path) {
    const std::string checksums_content = read_file_text(checksums_path);
    if (checksums_content.empty()) {
        return false;
    }

    const std::optional<std::string> expected_hash =
        parse_sha256sums_for_file(checksums_content, file_path.filename().string());
    if (!expected_hash.has_value()) {
        return false;
    }

    const std::optional<std::string> actual_hash = compute_sha256_hex(file_path);
    return actual_hash.has_value() && *actual_hash == *expected_hash;
}

}  // namespace updater
