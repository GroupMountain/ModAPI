#include "modapi/item/CreativeItemRegistry.h"
#include "modapi/core/RegistryEvent.h"
#include <atomic>
#include <ll/api/event/EventBus.h>
#include <ll/api/memory/Hook.h>
#include <ll/api/service/Bedrock.h>
#include <mc/world/item/VanillaItems.h>
#include <mc/world/item/registry/CreativeGroupInfo.h>
#include <mc/world/item/registry/CreativeItemEntry.h>
#include <mc/world/item/registry/CreativeItemGroupCategory.h>
#include <mc/world/item/registry/CreativeItemRegistry.h>
#include <mc/world/item/registry/ItemRegistry.h>
#include <mc/world/item/registry/ItemRegistryRef.h>
#include <mc/world/level/Level.h>
#include <utility>

namespace std {

template <>
class hash<CreativeItemNetId> {
public:
    size_t operator()(CreativeItemNetId const& netId) const { return std::hash<uint32_t>{}(netId.mRawId); }
};

} // namespace std

bool operator==(CreativeItemNetId const& lhs, CreativeItemNetId const& rhs) { return lhs.mRawId == rhs.mRawId; }

namespace modapi::inline item {

struct ServerInitCreativeItemsCallbackHook;

struct CreativeItemRegistry::Impl {
    ll::memory::HookRegistrar<ServerInitCreativeItemsCallbackHook> mHook;
    // The creative item registry vanilla built for the current pass. Vanilla constructs a new one
    // for every creative item pass, so this is refreshed by the hook below.
    ::CreativeItemRegistry* mRegistry = nullptr;
};


// `ItemRegistryRef::initCreativeItemsServer` builds a fresh CreativeItemRegistry and fills it by
// calling this callback. Binding and publishing after `origin()` means the deferred custom entries
// land in that same registry, in the pass that produced it.
LL_STATIC_HOOK(
    ServerInitCreativeItemsCallbackHook,
    HookPriority::Normal,
    &::VanillaItems::serverInitCreativeItemsCallback,
    void,
    ::ItemRegistryRef                                  itemRegistry,
    ::BlockDefinitionGroup const&                      blockDefinitionGroup,
    ::CreativeItemRegistry*                            creativeItemRegistry,
    ::BaseGameVersion const&                           worldVersion,
    ::Experiments const&                               experiments,
    ::ResourcePackManager const&                       resourcePackManager,
    ::cereal::ReflectionCtx const&                     ctx,
    ::Bedrock::NonOwnerPointer<::LinkedAssetValidator> validator,
    ::IMinecraftEventing&                              eventing
) {
    origin(
        itemRegistry,
        blockDefinitionGroup,
        creativeItemRegistry,
        worldVersion,
        experiments,
        resourcePackManager,
        ctx,
        std::move(validator),
        eventing
    );
    if (creativeItemRegistry != nullptr) {
        CreativeItemRegistry::getInstance()._bindRegistry(*creativeItemRegistry);
    }
}

CreativeItemRegistry::CreativeItemRegistry() : pImpl(std::make_unique<Impl>()) {}

CreativeItemRegistry& CreativeItemRegistry::getInstance() {
    // Deliberately leaked: the hook registrar this instance owns must not unregister its hooks
    // while the process/DLL is being torn down.
    static CreativeItemRegistry& instance = *new CreativeItemRegistry();
    return instance;
}

bool CreativeItemRegistry::isReady() const noexcept { return pImpl->mRegistry != nullptr; }

void CreativeItemRegistry::ensureEventRegistered() {
    // The vanilla hooks live in the registry's implementation, so the singleton has to exist before
    // the pass that fires them; every consumer goes through this entry point.
    (void)getInstance();
    static std::atomic_bool registered = false;
    core::ensureRegistryEventRegistered<EventType>(registered);
}

void CreativeItemRegistry::_bindRegistry(::CreativeItemRegistry& registry) {
    pImpl->mRegistry = &registry;
    ll::event::EventBus::getInstance().publish(CreativeItemReadyEvent{*this});
}

uint32_t CreativeItemRegistry::registerCreativeGroup(
    std::string_view                    groupName,
    ::ItemInstance&&                    icon,
    ::SharedTypes::CreativeItemCategory category
) {
    if (pImpl->mRegistry == nullptr) {
        core::getLogger().error("CreativeItemRegistry: the creative item registry is not available yet.");
        return 0;
    }
    try {
        // The engine owns the category objects and creates them in `createCategories()`; building one
        // with `try_emplace` produced a `CreativeItemGroupCategory` whose `EnableNonOwnerReferences`
        // base was never registered, and the engine wrote through that null list while shutting down.
        auto                         categories     = pImpl->mRegistry->createCategories();
        ::CreativeItemGroupCategory* categoryObject = nullptr;
        for (auto&& categoryEntry : categories) {
            if (categoryEntry.first == category) {
                categoryObject = categoryEntry.second;
                break;
            }
        }
        if (categoryObject == nullptr) {
            core::getLogger().error(
                "CreativeItemRegistry: the engine has no category {} to register a group in.",
                (int)category
            );
            return 0;
        }

        auto const name = ::HashedString(groupName);
        if (!groupName.empty()) {
            if (auto* existing = categoryObject->getChildGroup(name)) return existing->mIndex;
        }

        auto* group =
            groupName.empty() ? categoryObject->addAnonymousGroup() : categoryObject->addChildGroup(name, icon);
        return group != nullptr ? group->mIndex : 0;
    } catch (std::exception const& e) {
        core::getLogger()
            .error("CreativeItemRegistry: could not register the creative group '{}': {}", groupName, e.what());
        return 0;
    } catch (...) {
        core::getLogger().error("CreativeItemRegistry: could not register the creative group '{}'.", groupName);
        return 0;
    }
}

CreativeItemRegistry& CreativeItemReadyEvent::registry() const { return mRegistry; }

} // namespace modapi::inline item
