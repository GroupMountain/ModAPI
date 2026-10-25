#include "modapi/worldgen/SurfaceBuilderRegistry.h"
#include "modapi/core/RegistryEvent.h"
#include <atomic>
#include <ll/api/base/Containers.h>
#include <ll/api/event/EventBus.h>
#include <ll/api/memory/Hook.h>
#include <mc/world/level/LevelSeed64.h>
#include <mc/world/level/biome/VanillaBiomes.h>
#include <mc/world/level/biome/components/SurfaceBuilderData.h>
#include <mc/world/level/biome/registry/SurfaceBuilderRegistry.h>
#include <utility>

namespace modapi::inline worldgen {

// The engine fills its registry with the vanilla builders once per configured level; that call is
// the only moment the registry is handed to a function, so it is where ModAPI binds to it.
LL_STATIC_HOOK(
    VanillaBiomesInitSurfaceBuildersHook,
    HookPriority::Normal,
    &VanillaBiomes::initSurfaceBuilders,
    void,
    ::SurfaceBuilderRegistry&    registry,
    class BaseGameVersion const& baseGameVersion
) {
    origin(registry, baseGameVersion);
    SurfaceBuilderRegistry::getInstance()._bindRegistry(registry);
}

SurfaceBuilderRegistry& SurfaceBuilderReadyEvent::registry() const { return mRegistry; }

struct SurfaceBuilderRegistry::Impl {
    struct Entry {
        ::ISurfaceBuilder*   mBuilder = nullptr; // owned by the engine map
        ::SurfaceBuilderType mType{};
    };

    ::SurfaceBuilderRegistry*                                       mRegistry = nullptr;
    bool                                                            mReady    = false;
    ::std::atomic<bool>                                             mEventRegistered{false};
    unsigned short                                                  mNextType = 100; // the engine owns 0..7
    ll::DenseMap<::std::string, Entry>                              mEntries;
    ll::memory::HookRegistrar<VanillaBiomesInitSurfaceBuildersHook> mHook;
};

SurfaceBuilderRegistry::SurfaceBuilderRegistry() : pImpl(::std::make_unique<Impl>()) {}

SurfaceBuilderRegistry::~SurfaceBuilderRegistry() = default;

SurfaceBuilderRegistry& SurfaceBuilderRegistry::getInstance() {
    static SurfaceBuilderRegistry instance;
    return instance;
}

bool SurfaceBuilderRegistry::isReady() const noexcept { return pImpl->mReady; }

void SurfaceBuilderRegistry::ensureEventRegistered() {
    // Constructing the singleton installs the hook that binds the engine registry; the registrar
    // lives as long as the process.
    (void)getInstance();
    core::ensureRegistryEventRegistered<SurfaceBuilderReadyEvent>(getInstance().pImpl->mEventRegistered);
}

void SurfaceBuilderRegistry::_bindRegistry(::SurfaceBuilderRegistry& registry) {
    pImpl->mRegistry = &registry;
    if (pImpl->mReady) return;
    pImpl->mReady = true;
    ll::event::EventBus::getInstance().publish(SurfaceBuilderReadyEvent{*this});
}

SurfaceBuilderRegistry::ProductRef
SurfaceBuilderRegistry::_registerBuilder(::std::string_view identifier, ::std::unique_ptr<::ISurfaceBuilder> builder) {
    if (pImpl->mRegistry == nullptr || !builder) return {};
    if (pImpl->mEntries.contains(identifier)) return {};

    auto* raw = builder.get();
    auto  key = static_cast<::SurfaceBuilderType>(pImpl->mNextType++);
    // `std::map` keyed by `SurfaceBuilderType`: the engine keeps the first builder for a key, and a
    // key is only reused if a previous reservation was never taken.
    auto [it, inserted] = pImpl->mRegistry->mSurfaceBuilders->emplace(key, ::std::move(builder));
    if (!inserted) return {};
    (void)it;
    // The engine map owns the object now; remember the raw pointer and the key only.
    pImpl->mEntries.emplace(::std::string{identifier}, Impl::Entry{raw, key});
    return raw;
}

SurfaceBuilderRegistry::ProductRef SurfaceBuilderRegistry::getBuilder(::std::string_view identifier) const {
    if (pImpl->mRegistry == nullptr) return {};
    auto it = pImpl->mEntries.find(identifier);
    if (it == pImpl->mEntries.end()) return {};
    auto mapIt = pImpl->mRegistry->mSurfaceBuilders->find(it->second.mType);
    if (mapIt == pImpl->mRegistry->mSurfaceBuilders->end()) return {};
    return mapIt->second.get();
}

} // namespace modapi::inline worldgen
