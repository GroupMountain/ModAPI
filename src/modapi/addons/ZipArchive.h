#pragma once
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace modapi::inline addons {

// Minimal read-only zip reader built directly on minizip-ng.
// Replaces gmlib::zip_utils::Unzipper, which AddonsLoader used to extract addon packs.

class ZipArchive {
    struct Impl;
    std::unique_ptr<Impl> pImpl;

public:
    explicit ZipArchive(std::filesystem::path const& path);
    ~ZipArchive();

    ZipArchive(ZipArchive const&)            = delete;
    ZipArchive(ZipArchive&&)                 = delete;
    ZipArchive& operator=(ZipArchive const&) = delete;

    [[nodiscard]] bool isOpen() const;

    [[nodiscard]] std::vector<std::string> entryNames() const;

    // Entries whose path escapes `dest` (absolute or containing "..") are skipped.
    bool extractAll(std::filesystem::path const& dest, bool replace = false) const;
};

} // namespace modapi::inline addons
