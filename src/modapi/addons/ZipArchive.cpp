#include "modapi/addons/ZipArchive.h"
#include "modapi/core/FileUtils.h"
#include <array>
#include <minizip/mz.h>
#include <minizip/mz_strm.h>
#include <minizip/mz_strm_os.h>
#include <minizip/mz_zip.h>
#include <optional>
#include <string_view>
#include <system_error>

namespace modapi::inline addons {

namespace {

// Zip entry names always use '/' as separator, but some writers emit '\' as well.
bool isDirectoryEntry(std::string_view name) { return name.empty() || name.ends_with('/') || name.ends_with('\\'); }

// Rejects entry names that would escape `dest` (absolute paths, drive roots, ".." components).
std::optional<std::filesystem::path> resolveEntryPath(std::filesystem::path const& dest, std::string_view name) {
    auto relative = std::filesystem::path(name).lexically_normal();
    if (relative.is_absolute() || relative.has_root_name() || relative.has_root_directory()) return std::nullopt;
    for (auto const& part : relative) {
        if (part == "..") return std::nullopt;
    }
    return dest / relative;
}

// minizip-ng's OS stream takes a narrow path; entry names are UTF-8, so the path is passed as UTF-8 bytes.
std::string toNarrowUtf8(std::filesystem::path const& path) {
    auto const u8 = path.u8string();
    return {u8.begin(), u8.end()};
}

} // namespace

struct ZipArchive::Impl {
    void* mStream     = nullptr;
    void* mHandle     = nullptr;
    bool  mStreamOpen = false;
    bool  mOpen       = false;

    explicit Impl(std::filesystem::path const& path) {
        std::error_code ec;
        if (!std::filesystem::is_regular_file(path, ec)) return;

        mStream = mz_stream_os_create();
        if (mStream == nullptr) return;
        if (mz_stream_os_open(mStream, toNarrowUtf8(path).c_str(), MZ_OPEN_MODE_READ) != MZ_OK) return;
        mStreamOpen = true;

        mHandle = mz_zip_create();
        if (mHandle == nullptr) return;
        if (mz_zip_open(mHandle, mStream, MZ_OPEN_MODE_READ) != MZ_OK) return;

        mOpen = true;
    }

    ~Impl() {
        if (mHandle != nullptr) {
            if (mOpen) {
                if (mz_zip_entry_is_open(mHandle) == MZ_OK) mz_zip_entry_close(mHandle);
                mz_zip_close(mHandle);
            }
            mz_zip_delete(&mHandle);
        }
        if (mStream != nullptr) {
            if (mStreamOpen) mz_stream_close(mStream);
            mz_stream_os_delete(&mStream);
        }
    }
};

ZipArchive::ZipArchive(std::filesystem::path const& path) : pImpl(std::make_unique<Impl>(path)) {}

ZipArchive::~ZipArchive() = default;

bool ZipArchive::isOpen() const { return pImpl->mOpen; }

std::vector<std::string> ZipArchive::entryNames() const {
    std::vector<std::string> names;
    if (!pImpl->mOpen) return names;
    if (mz_zip_goto_first_entry(pImpl->mHandle) != MZ_OK) return names;

    do {
        mz_zip_file* info = nullptr;
        if (mz_zip_entry_get_info(pImpl->mHandle, &info) != MZ_OK || info == nullptr) break;
        if (info->filename != nullptr) names.emplace_back(info->filename);
    } while (mz_zip_goto_next_entry(pImpl->mHandle) == MZ_OK);

    return names;
}

bool ZipArchive::extractAll(std::filesystem::path const& dest, bool replace) const {
    if (!pImpl->mOpen) return false;

    std::error_code ec;
    if (!std::filesystem::exists(dest, ec)) {
        std::filesystem::create_directories(dest, ec);
        if (ec) return false;
    }
    // Same behaviour as the gmlib::zip_utils::Unzipper this replaces: without `replace`, an
    // already populated directory is left untouched.
    if (!replace && !std::filesystem::is_empty(dest, ec)) return true;

    if (mz_zip_goto_first_entry(pImpl->mHandle) != MZ_OK) return false;

    bool                        result = true;
    std::array<char, 64 * 1024> buffer{};
    do {
        mz_zip_file* info = nullptr;
        if (mz_zip_entry_get_info(pImpl->mHandle, &info) != MZ_OK || info == nullptr) {
            result = false;
            break;
        }

        std::string const name   = info->filename != nullptr ? info->filename : std::string{};
        auto const        target = resolveEntryPath(dest, name);
        if (!target) continue; // entry escapes the destination directory, skip it

        if (isDirectoryEntry(name)) {
            std::filesystem::create_directories(*target, ec);
            continue;
        }

        if (mz_zip_entry_read_open(pImpl->mHandle, 0, nullptr) != MZ_OK) {
            result = false;
            break;
        }

        // `uncompressed_size` is not trusted: read until the entry reports end of stream.
        std::string content;
        for (int32_t read = mz_zip_entry_read(pImpl->mHandle, buffer.data(), static_cast<int32_t>(buffer.size()));
             read > 0;
             read = mz_zip_entry_read(pImpl->mHandle, buffer.data(), static_cast<int32_t>(buffer.size()))) {
            content.append(buffer.data(), static_cast<size_t>(read));
        }
        mz_zip_entry_close(pImpl->mHandle);

        if (!core::writeFile(*target, content, true)) result = false;
    } while (mz_zip_goto_next_entry(pImpl->mHandle) == MZ_OK);

    return result;
}

} // namespace modapi::inline addons
