#pragma once

#include <random>
#include <sstream>
#include <string>
#include <iomanip>

// UUID v4 generator without external dependencies.
inline std::string generate_uuid() {
    static thread_local std::mt19937 gen{std::random_device{}()};
    std::uniform_int_distribution<int> dist(0, 15);
    std::uniform_int_distribution<int> dist2(8, 11);

    std::ostringstream ss;
    ss << std::hex;
    for (int i = 0; i < 8; ++i) ss << dist(gen);
    ss << '-';
    for (int i = 0; i < 4; ++i) ss << dist(gen);
    ss << "-4";
    for (int i = 0; i < 3; ++i) ss << dist(gen);
    ss << '-';
    ss << dist2(gen);
    for (int i = 0; i < 3; ++i) ss << dist(gen);
    ss << '-';
    for (int i = 0; i < 12; ++i) ss << dist(gen);

    return ss.str();
}
