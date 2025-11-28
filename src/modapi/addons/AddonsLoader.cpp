#include "modapi/addons/AddonsLoader.h"
#include "modapi/core/Gloabl.h"
#include <gmlib/gm/io/ZipUtils.h>
#include <ll/api/memory/Hook.h>
#include <ll/api/service/Bedrock.h>
#include <ll/api/thread/ThreadPoolExecutor.h>
#include <magic_enum.hpp>
#include <mc/deps/core/debug/log/LogArea.h>
#include <mc/deps/core/debug/log/LogLevel.h>
#include <mc/deps/core/file/Path.h>
#include <mc/resources/CompositePackSource.h>
#include <mc/resources/DirectoryPackSource.h>
#include <mc/resources/IRepositoryFactory.h>
#include <mc/resources/Pack.h>
#include <mc/resources/PackInstance.h>
#include <mc/resources/PackManifest.h>
#include <mc/resources/PackSettingsFactory.h>
#include <mc/resources/PackSourceFactory.h>
#include <mc/resources/RepositorySources.h>
#include <mc/resources/ResourcePack.h>
#include <mc/resources/ResourcePackRepository.h>
#include <mc/resources/ResourcePackStack.h>

namespace modapi::inline addons {

struct AddonsLoader::Impl {
public:
    struct PacksCtorHook;
    struct PacksLoadHook;
    struct RepositorySourcesInitHook;
    struct ResourcePackRepositoryInitHook;
    struct FuckMultipleManifestOutput;

public:
    ll::memory::HookRegistrar<
        PacksCtorHook,
        PacksLoadHook,
        RepositorySourcesInitHook,
        ResourcePackRepositoryInitHook,
        FuckMultipleManifestOutput>
                                        mHooks;
    ResourcePackRepository*             mResourcePackRepository;
    ll::DenseSet<std::filesystem::path> mAllResourcePath;
    std::vector<std::string>            mPackListCache;
    std::vector<std::thread>            mDecompressThread;
    std::filesystem::path               mResourceCachePath = getSelfMod().getModDir() / ".cache" / "addons";

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
            if (entry.mName.ends_with("/manifest.json")) return true;
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
    const_cast<CompositePackSource&>(*repo.mSources->mWorldPackSource)
        .mPackSources->push_back(
            &repo.getPackSourceFactory().createDirectoryPackSource(Core::Path(path), type, PackOrigin::Test, false)
        );
    repo.refreshPacks();
}

LL_TYPE_INSTANCE_HOOK(
    AddonsLoader::Impl::PacksCtorHook,
    HookPriority::Normal,
    ResourcePack,
    &ResourcePack::$ctor,
    void*,
    gsl::not_null<std::shared_ptr<Pack>> pack
) {
    auto* result = origin(std::move(pack));
    if (auto& manifest = mPack->mManifest; manifest && manifest->mPackOrigin == PackOrigin::Test) {
        AddonsLoader::getInstance().pImpl->mPackListCache.push_back(pack->mManifest->mIdentity->asString());
    }
    return result;
}

LL_STATIC_HOOK(
    AddonsLoader::Impl::PacksLoadHook,
    HookPriority::Normal,
    &ResourcePackStack::deserialize,
    std::unique_ptr<ResourcePackStack>,
    std::istream&                                                                 fileStream,
    gsl::not_null<Bedrock::NonOwnerPointer<IResourcePackRepository const>> const& repo,
    std::optional<std::string>                                                    levelId
) {
    auto& impl                   = AddonsLoader::getInstance().pImpl;
    auto  resourcePackRepository = ll::service::getResourcePackRepository();

    for (auto& thread : impl->mDecompressThread) {
        if (thread.joinable()) thread.join();
    }

    auto stack = origin(fileStream, repo, std::move(levelId));
    for (auto& id : impl->mPackListCache) {
        auto pack = resourcePackRepository->getResourcePackForPackId(PackIdVersion::fromString(id));
        stack->add(
            PackInstance(
                {pack},
                0,
                false,
                resourcePackRepository->getPackSettingsFactory().getPackSettings(*pack->mPack->mManifest, std::nullopt)
            ),
            repo,
            false
        );
    }
    impl->mPackListCache.clear();
    return std::move(stack);
}

LL_TYPE_INSTANCE_HOOK(
    AddonsLoader::Impl::ResourcePackRepositoryInitHook,
    HookPriority::Normal,
    ResourcePackRepository,
    &ResourcePackRepository::$ctor,
    void*,
    gsl::not_null<std::shared_ptr<RepositoryPacks>>                   repositoryPacks,
    PackManifestFactory&                                              manifestFactory,
    Bedrock::NotNullNonOwnerPtr<IContentAccessibilityProvider> const& contentAccessibility,
    Bedrock::NotNullNonOwnerPtr<Core::FilePathManager> const&         pathManager,
    Bedrock::NonOwnerPointer<PackCommand::IPackCommandPipeline>       commands,
    PackSourceFactory&                                                packSourceFactory,
    bool                                                              initAsync,
    std::unique_ptr<IRepositoryFactory>                               factory
) {
    AddonsLoader::getInstance().pImpl->mResourcePackRepository = this;
    return origin(
        std::move(repositoryPacks),
        manifestFactory,
        contentAccessibility,
        pathManager,
        std::move(commands),
        packSourceFactory,
        initAsync,
        std::move(factory)
    );
}

LL_TYPE_INSTANCE_HOOK(
    AddonsLoader::Impl::RepositorySourcesInitHook,
    HookPriority::Normal,
    RepositorySources,
    &RepositorySources::initializePackSource,
    void,
    PackSourceFactory& packSourceFactory
) {
    origin(packSourceFactory);
    auto& impl = AddonsLoader::getInstance().pImpl;

    for (auto [type, name] : magic_enum::enum_entries<PackType>()) {
        if (type != PackType::Invalid && type != PackType::Count) {
            for (auto& path : impl->mAllResourcePath) {
                mWorldPackSource->mPackSources->push_back(
                    &packSourceFactory.createDirectoryPackSource(Core::Path(path), type, PackOrigin::Test, false)
                );
                impl->mResourcePackRepository->refreshPacks();
            }
        }
    }
}

using namespace ll::memory_literals;

LL_STATIC_HOOK(
    AddonsLoader::Impl::FuckMultipleManifestOutput,
    HookPriority::Normal,
    // ContentLogHelper::_contentLog<char const (&)[135]>
    "48 89 5C 24 ?? 48 89 74 24 ?? 57 48 83 EC ?? 48 8B 05 ?? ?? ?? ?? 48 33 C4 48 89 44 24 ?? 49 8B F1 41 8B D8 8B FA"_sig,
    void,
    bool        logOnlyOnce,
    LogLevel    level,
    LogArea     area,
    char const* message
) {
    constexpr static std::string_view fuckMessage =
        "Cannot determine which pack manifest to use: Multiple manifests found at the "
        "same directory level in the pack's folder hierarchy.";
    if (message == fuckMessage) return;
    origin(logOnlyOnce, level, area, message);
}

} // namespace modapi::inline addons