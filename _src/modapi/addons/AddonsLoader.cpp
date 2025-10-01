// #include "gmlib/mod/addons/AddonsLoader.h"
// #include "core/Global.h"
// #include "magic_enum.hpp"
// #include "gmlib/gm/io/ZipUtils.h"
// #include <mc/deps/core/file/Path.h>
// #include <mc/deps/core/resource/PackIdVersion.h>
// #include <mc/deps/core/resource/PackOrigin.h>
// #include <mc/deps/core/utility/NonOwnerPointer.h>
// #include <mc/resources/CompositePackSource.h>
// #include <mc/resources/DirectoryPackSource.h>
// #include <mc/resources/Pack.h>
// #include <mc/resources/PackInstance.h>
// #include <mc/resources/PackManifest.h>
// #include <mc/resources/PackSettings.h>
// #include <mc/resources/PackSettingsFactory.h>
// #include <mc/resources/PackSource.h>
// #include <mc/resources/PackSourceFactory.h>
// #include <mc/resources/ResourcePack.h>
// #include <mc/resources/ResourcePackRepository.h>
// #include <mc/resources/ResourcePackStack.h>
// #include <memory>
// #include <thread>

// namespace gmlib::mod::AddonsLoader {

// std::vector<std::filesystem::path> mAllResourcePath;
// std::vector<std::string>           mPackListCache;
// std::vector<std::thread>           mDecompressThread;
// std::filesystem::path              mResourceCachePath = getSelfMod().getModDir() / ".cache" / "addons";

// void addResourcePackPath(ResourcePackRepository& repo, PackType type) {
//     auto* compositePack     = const_cast<CompositePackSource*>(repo.getWorldPackSource());
//     auto& packSourceFactory = repo.getPackSourceFactory();
//     for (auto& path : mAllResourcePath) {
//         compositePack->mPackSources->push_back(
//             &packSourceFactory.createDirectoryPackSource(Core::Path(path), type, PackOrigin::Test, 0)
//         );
//     }
//     repo.refreshPacks();
// }

// LL_TYPE_INSTANCE_HOOK(
//     PacksBuildEvent,
//     ll::memory::HookPriority::Normal,
//     ResourcePack,
//     &ResourcePack::$ctor,
//     void*,
//     Pack& pack
// ) {
//     auto* result   = origin(pack);
//     auto* manifest = &getManifest();
//     if (manifest && manifest->mPackOrigin == PackOrigin::Test) {
//         mPackListCache.push_back(pack.mManifest->mIdentity->asString());
//     }
//     return result;
// }

// LL_STATIC_HOOK(
//     PacksLoadEvent,
//     ll::memory::HookPriority::Normal,
//     &ResourcePackStack::deserialize,
//     std::unique_ptr<ResourcePackStack>,
//     std::istream&                                                                 fileStream,
//     gsl::not_null<Bedrock::NonOwnerPointer<IResourcePackRepository const>> const& repo
// ) {
//     for (auto& thread : mDecompressThread) {
//         if (thread.joinable()) thread.join();
//     }
//     auto stack = origin(fileStream, repo);
//     for (auto id : mPackListCache) {
//         auto  resourcePackRepository = ll::service::getResourcePackRepository();
//         auto* pack                   =
//         resourcePackRepository->getResourcePackForPackId(PackIdVersion::fromString(id)); stack->add(
//             PackInstance(
//                 gsl::not_null<Bedrock::NonOwnerPointer<ResourcePack>>(Bedrock::NonOwnerPointer(*pack)),
//                 0,
//                 false,
//                 resourcePackRepository->getPackSettingsFactory().getPackSettings(pack->getManifest())
//             ),
//             repo,
//             0
//         );
//     }
//     mPackListCache.clear();
//     return stack;
// }

// LL_TYPE_INSTANCE_HOOK(
//     ResourcePackRepositoryInitEvent,
//     ll::memory::HookPriority::Normal,
//     ResourcePackRepository,
//     &ResourcePackRepository::_initialize,
//     void
// ) {
//     for (auto [type, name] : magic_enum::enum_entries<PackType>()) {
//         if (type != PackType::Invalid && type != PackType::Count) {
//             addResourcePackPath(*this, type);
//         }
//     }
//     return origin();
// }

// struct CustomPack_Impl {
//     ll::memory::HookRegistrar<PacksBuildEvent, ResourcePackRepositoryInitEvent, PacksLoadEvent> r;
// };

// std::unique_ptr<CustomPack_Impl> impl;

// void addCustomPackPath(std::filesystem::path const& path) {
//     if (!impl) impl = std::make_unique<CustomPack_Impl>();
//     if (!std::filesystem::exists(path)) {
//         std::filesystem::create_directories(path);
//     }
//     mAllResourcePath.push_back(path);

//     for (auto& entry : std::filesystem::directory_iterator(path)) {
//         if (!entry.is_regular_file()) continue;
//         auto zip = std::make_shared<zip_utils::Unzipper>(entry.path());
//         if (!zip->isOpen()) continue;
//         bool result = false;
//         for (auto& it : *zip) {
//             if (!it.mName.ends_with("manifest.json")) continue;
//             if (std::count(it.mName.begin(), it.mName.end(), '/') == 1) {
//                 result = true;
//                 break;
//             }
//         }
//         if (!result) continue;
//         mDecompressThread.emplace_back([zip, name = entry.path().stem().string()]() -> void {
//             mAllResourcePath.push_back(mResourceCachePath / name);
//             zip->extractAll(mResourceCachePath / name, true);
//         });
//     }
// }

// void setCustomPackPath(ResourcePackRepository& repo, std::filesystem::path const& path, PackType type) {
//     const_cast<CompositePackSource*>(repo.getWorldPackSource())
//         ->mPackSources->push_back(
//             &repo.getPackSourceFactory().createDirectoryPackSource(Core::Path(path), type, PackOrigin::Test, 0)
//         );
//     repo.refreshPacks();
// }

// } // namespace gmlib::mod::AddonsLoader

#include "modapi/addons/AddonsLoader.h"
#include "modapi/core/Gloabl.h"
#include <ll/api/thread/ThreadPoolExecutor.h>

namespace modapi::inline addons {

struct AddonsLoader::Impl {
public:
    ll::thread::ThreadPoolExecutor mThreadPool{
        fmt::format("{0}-{1}", getSelfMod().getName(), ll::reflection::type_stem_name_v<AddonsLoader>)
    };
    std::vector<std::filesystem::path> mAllResourcePath;
    std::vector<std::string>           mPackListCache;
    std::vector<std::thread>           mDecompressThread;
    std::filesystem::path              mResourceCachePath = getSelfMod().getModDir() / ".cache" / "addons";

public:
    Impl() {}
    ~Impl() {}
};

AddonsLoader::AddonsLoader() : mImpl(std::make_unique<Impl>()) {}
AddonsLoader::~AddonsLoader() = default;

void AddonsLoader::addCustomPackPath(std::filesystem::path const& path) {}

void AddonsLoader::setCustomPackPath(ResourcePackRepository& repo, std::filesystem::path const& path, PackType type) {}

} // namespace modapi::inline addons