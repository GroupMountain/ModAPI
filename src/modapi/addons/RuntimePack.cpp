#include "modapi/addons/RuntimePack.h"
#include "modapi/addons/AddonsLoader.h"
#include "modapi/core/Gloabl.h"
#include <fstream>
#include <random>
#include <sstream>
#include <system_error>

namespace modapi::inline addons {
namespace {

std::mt19937_64& randomEngine() {
    static std::mt19937_64 engine{std::random_device{}()};
    return engine;
}

std::string randomHex(size_t digits) {
    std::uniform_int_distribution<int> distribution(0, 15);
    static char const*                 digits_ = "0123456789abcdef";

    std::string out;
    out.reserve(digits);
    for (size_t i = 0; i < digits; ++i) out.push_back(digits_[distribution(randomEngine())]);
    return out;
}

// Fresh every time on purpose: a client that already has a pack with this uuid (from an earlier run of the
// server, say) will not download it again, so every run has to offer an identity it has never seen. Keeping
// the uuid stable and bumping the version would be the tidier way to update a pack a client already has, but
// it does not help while the pack is being rebuilt from scratch on each start.
std::string randomUuid() {
    auto const hex = randomHex(32);
    return hex.substr(0, 8) + "-" + hex.substr(8, 4) + "-4" + hex.substr(13, 3) + "-a" + hex.substr(17, 3) + "-"
         + hex.substr(20, 12);
}

std::string escape(std::string const& text) {
    std::string out;
    for (char c : text) {
        switch (c) {
        case '"':
            out += "\\\"";
            break;
        case '\\':
            out += "\\\\";
            break;
        default:
            out.push_back(c);
        }
    }
    return out;
}

bool writeFile(std::filesystem::path const& path, std::string const& content) {
    std::error_code ec;
    std::filesystem::create_directories(path.parent_path(), ec);

    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out) return false;

    out << content;
    return out.good();
}

std::string manifest(
    std::string const& name,
    std::string const& moduleType,
    std::string const& headerUuid,
    std::string const& moduleUuid
) {
    std::ostringstream os;
    os << "{\n  \"format_version\": 2,\n  \"header\": {\n"
       << "    \"name\": \"" << escape(name) << "\",\n"
       << "    \"description\": \"Built at runtime by ModAPI\",\n"
       << "    \"uuid\": \"" << headerUuid << "\",\n"
       << "    \"version\": [1, 0, 0],\n"
       << "    \"min_engine_version\": [1, 21, 0]\n  },\n"
       << "  \"modules\": [{\"type\": \"" << escape(moduleType) << "\", \"uuid\": \"" << moduleUuid
       << "\", \"version\": [1, 0, 0]}]\n}\n";
    return os.str();
}

} // namespace

struct RuntimePack::Impl {
    std::string           mName;
    std::string           mModuleType;
    std::string           mHeaderUuid;
    std::string           mModuleUuid;
    std::filesystem::path mRoot;      // the directory holding this one pack
    std::filesystem::path mContainer; // what gets handed to the engine (a directory of packs)
    bool                  mInstalled = false;
};

RuntimePack::RuntimePack(std::string name, std::string moduleType) : pImpl(std::make_unique<Impl>()) {
    pImpl->mName       = std::move(name);
    pImpl->mModuleType = std::move(moduleType);
    pImpl->mHeaderUuid = randomUuid();
    pImpl->mModuleUuid = randomUuid();
}

RuntimePack::~RuntimePack() = default;

std::filesystem::path const& RuntimePack::root() const { return pImpl->mRoot; }

RuntimePack& RuntimePack::addFile(std::filesystem::path const& relativePath, std::string const& content) {
    if (pImpl->mRoot.empty()) {
        // A fresh directory per pack, under the system temp directory.
        pImpl->mContainer = std::filesystem::temp_directory_path() / ("modapi_packs_" + randomHex(8));
        pImpl->mRoot      = pImpl->mContainer / pImpl->mName;
    }

    if (!writeFile(pImpl->mRoot / relativePath, content)) {
        core::getLogger().error("RuntimePack[{}]: could not write '{}'.", pImpl->mName, relativePath.string());
    }
    return *this;
}

RuntimePack& RuntimePack::addFileFrom(std::filesystem::path const& relativePath, std::filesystem::path const& source) {
    if (pImpl->mRoot.empty()) {
        pImpl->mContainer = std::filesystem::temp_directory_path() / ("modapi_packs_" + randomHex(8));
        pImpl->mRoot      = pImpl->mContainer / pImpl->mName;
    }

    auto const target = pImpl->mRoot / relativePath;

    std::error_code ec;
    std::filesystem::create_directories(target.parent_path(), ec);
    std::filesystem::copy_file(source, target, std::filesystem::copy_options::overwrite_existing, ec);
    if (ec) {
        core::getLogger().error(
            "RuntimePack[{}]: could not copy '{}' to '{}': {}",
            pImpl->mName,
            source.string(),
            target.string(),
            ec.message()
        );
    }
    return *this;
}

bool RuntimePack::install() {
    if (pImpl->mRoot.empty()) {
        core::getLogger().warn("RuntimePack[{}]: nothing was added, nothing installed.", pImpl->mName);
        return false;
    }
    if (pImpl->mInstalled) return true;

    if (!writeFile(
            pImpl->mRoot / "manifest.json",
            manifest(pImpl->mName, pImpl->mModuleType, pImpl->mHeaderUuid, pImpl->mModuleUuid)
        )) {
        return false;
    }

    pImpl->mInstalled = true;

    // The engine's own addon loading: it turns the directory into a pack source, loads the packs it finds
    // and puts them into the stack clients are sent.
    AddonsLoader::getInstance().addCustomPackPath(pImpl->mContainer);

    core::getLogger().info(
        "RuntimePack[{}]: installed as a '{}' pack with uuid {} from '{}'.",
        pImpl->mName,
        pImpl->mModuleType,
        pImpl->mHeaderUuid,
        pImpl->mRoot.string()
    );
    return true;
}

} // namespace modapi::inline addons
