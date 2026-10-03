#pragma once
#include <filesystem>
#include <fstream>
#include <iterator>
#include <optional>
#include <string>
#include <string_view>

namespace modapi::inline core {

// Internal file helpers. ModAPI used to depend on gmlib::file_utils for these two calls.
// Both return an empty optional / false instead of throwing.

inline std::optional<std::string> readFile(std::filesystem::path const& filePath, bool isBinary = false) {
    if (!std::filesystem::exists(filePath)) return std::nullopt;

    std::ios_base::openmode mode = std::ios_base::in;
    if (isBinary) mode |= std::ios_base::binary;

    std::ifstream input(filePath, mode);
    if (!input.is_open()) return std::nullopt;

    std::string data{std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
    return data;
}

inline bool writeFile(std::filesystem::path const& filePath, std::string_view content, bool isBinary = false) {
    if (auto const& parent = filePath.parent_path(); !parent.empty() && !std::filesystem::exists(parent)) {
        std::filesystem::create_directories(parent);
    }

    std::ios_base::openmode mode = std::ios_base::out;
    if (isBinary) mode |= std::ios_base::binary;

    std::ofstream output(filePath, mode);
    if (!output.is_open()) return false;

    output.write(content.data(), static_cast<std::streamsize>(content.size()));
    return output.good();
}

} // namespace modapi::inline core
