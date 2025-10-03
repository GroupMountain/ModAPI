#include "modapi/addons/AddonsLoader.h"
#include "modapi/core/Gloabl.h"
#include <gmlib/gm/io/ZipUtils.h>
#include <ll/api/memory/Hook.h>
#include <ll/api/service/Bedrock.h>
#include <ll/api/thread/ThreadPoolExecutor.h>
#include <magic_enum.hpp>
#include <mc/deps/core/file/Path.h>
#include <mc/resources/CompositePackSource.h>
#include <mc/resources/DirectoryPackSource.h>
#include <mc/resources/Pack.h>
#include <mc/resources/PackInstance.h>
#include <mc/resources/PackManifest.h>
#include <mc/resources/PackSettingsFactory.h>
#include <mc/resources/PackSourceFactory.h>
#include <mc/resources/ResourcePack.h>
#include <mc/resources/ResourcePackRepository.h>
#include <mc/resources/ResourcePackStack.h>

namespace modapi::inline addons {

struct AddonsLoader::Impl {
public:
    struct PacksCtorHook;
    struct PacksLoadHook;
    struct ResourcePackRepositoryInitHook;

public:
    ll::memory::HookRegistrar<PacksCtorHook, PacksLoadHook, ResourcePackRepositoryInitHook> mHooks;
    ll::DenseSet<std::filesystem::path>                                                     mAllResourcePath;
    std::vector<std::string>                                                                mPackListCache;
    std::vector<std::thread>                                                                mDecompressThread;
    std::filesystem::path mResourceCachePath = getSelfMod().getModDir() / ".cache" / "addons";

public:
    Impl() {
        std::error_code errorCode;
        std::filesystem::remove_all(mResourceCachePath, errorCode);
    }
    ~Impl() = default;
};

AddonsLoader::AddonsLoader() : pImpl(std::make_unique<Impl>()) {}
AddonsLoader::~AddonsLoader() = default;

AddonsLoader& AddonsLoader::getInstance() {
    static AddonsLoader instance;
    return instance;
}

void AddonsLoader::addCustomPackPath(std::filesystem::path const& path) {
    constexpr static auto anyOfZip = [](auto&& zip) -> bool {
        for (auto& entry : *zip) {
            if (entry.mName.ends_with("manifest.json")
                && std::find(entry.mName.begin() + 1, entry.mName.end(), '/') == entry.mName.end()) {
                return true;
            }
        }
        return false;
    };

    if (!std::filesystem::exists(path)) {
        std::filesystem::create_directories(path);
    } else if (!std::filesystem::is_directory(path)) {
        throw std::runtime_error("path is not a directory");
    }

    pImpl->mAllResourcePath.insert(path);
    for (auto& entry : std::filesystem::directory_iterator(path)) {
        if (!entry.is_regular_file()) continue;
        auto zip = std::make_shared<gmlib::zip_utils::Unzipper>(entry.path());
        if (!zip->isOpen() || !anyOfZip(zip)) continue;
        pImpl->mDecompressThread.emplace_back([zip, name = entry.path().stem().string(), this]() -> void {
            auto path = pImpl->mResourceCachePath / name;
            pImpl->mAllResourcePath.insert(path);
            zip->extractAll(path, true);
        });
    }
}

void AddonsLoader::setCustomPackPath(ResourcePackRepository& repo, std::filesystem::path const& path, PackType type) {
    const_cast<CompositePackSource*>(repo.getWorldPackSource())
        ->mPackSources->push_back(
            &repo.getPackSourceFactory().createDirectoryPackSource(Core::Path(path), type, PackOrigin::Test, 0)
        );
    repo.refreshPacks();
}

LL_TYPE_INSTANCE_HOOK(
    AddonsLoader::Impl::PacksCtorHook,
    ll::memory::HookPriority::Normal,
    ResourcePack,
    &ResourcePack::$ctor,
    void*,
    Pack& pack
) {
    auto* result = origin(pack);
    if (auto* manifest = &getManifest(); manifest && manifest->mPackOrigin == PackOrigin::Test) {
        AddonsLoader::getInstance().pImpl->mPackListCache.push_back(pack.mManifest->mIdentity->asString());
    }
    return result;
}

LL_STATIC_HOOK(
    AddonsLoader::Impl::PacksLoadHook,
    ll::memory::HookPriority::Normal,
    &ResourcePackStack::deserialize,
    std::unique_ptr<ResourcePackStack>,
    std::istream&                                                                 fileStream,
    gsl::not_null<Bedrock::NonOwnerPointer<IResourcePackRepository const>> const& repo
) {
    auto& impl                   = AddonsLoader::getInstance().pImpl;
    auto  resourcePackRepository = ll::service::getResourcePackRepository();

    for (auto& thread : impl->mDecompressThread) {
        if (thread.joinable()) thread.join();
    }

    auto stack = origin(fileStream, repo);
    for (auto& id : impl->mPackListCache) {
        auto* pack = resourcePackRepository->getResourcePackForPackId(PackIdVersion::fromString(id));
        stack->add(
            PackInstance(
                {*pack},
                0,
                false,
                resourcePackRepository->getPackSettingsFactory().getPackSettings(pack->getManifest(), std::nullopt)
            ),
            repo,
            0
        );
    }
    impl->mPackListCache.clear();
    return std::move(stack);
}

LL_TYPE_INSTANCE_HOOK(
    AddonsLoader::Impl::ResourcePackRepositoryInitHook,
    ll::memory::HookPriority::Normal,
    ResourcePackRepository,
    &ResourcePackRepository::_initialize,
    void
) {
    auto& impl              = AddonsLoader::getInstance().pImpl;
    auto* compositePack     = const_cast<CompositePackSource*>(getWorldPackSource());
    auto& packSourceFactory = getPackSourceFactory();

    for (auto [type, name] : magic_enum::enum_entries<PackType>()) {
        if (type != PackType::Invalid && type != PackType::Count) {
            for (auto& path : impl->mAllResourcePath) {
                compositePack->mPackSources->push_back(
                    &packSourceFactory.createDirectoryPackSource(Core::Path(path), type, PackOrigin::Test, 0)
                );
            }
        }
    }
    refreshPacks();
    return origin();
}

} // namespace modapi::inline addons