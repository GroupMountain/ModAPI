#pragma once
#include "modapi/Macros.h"
#include "modapi/Registry.h"
#include "modapi/item/base/ICustomArmorItem.h"
#include "modapi/item/base/ICustomItem.h"
#include "modapi/item/shared_types/ItemInitializer.h"
#include "modapi/item/shared_types/NetworkTagBuilder.h"
#include <concepts>
#include <functional>
#include <ll/api/event/Event.h>
#include <mc/common/WeakPtr.h>
#include <mc/deps/core/utility/optional_ref.h>
#include <mc/world/item/Item.h>
#include <memory>
#include <string_view>
#include <type_traits>
#include <utility>

class ItemRegistry;

namespace modapi::inline item {

class ItemRegistry;

// Published at the end of `VanillaItems::registerItems`, i.e. after vanilla registered its items
// and before `ItemRegistry::_movePreRegistryToMainRegistry` moves them into the main registry.
//
// Items added after that point never reach the main registry nor the client item definition packet,
// so this is the only window in which custom items can be injected.
class ItemReadyEvent final : public ll::event::Event {
    ItemRegistry& mRegistry;

public:
    constexpr explicit ItemReadyEvent(ItemRegistry& registry) : mRegistry(registry) {}

    MOD_NDAPI ItemRegistry& registry() const;
};

class ItemRegistry {
public:
    struct Impl;
    std::unique_ptr<Impl> pImpl;

public:
    using EventType  = ItemReadyEvent;
    using Product    = ::Item;
    using ProductRef = optional_ref<::Item>;

public:
    ItemRegistry();
    ItemRegistry& operator=(ItemRegistry const&) = delete;
    ItemRegistry(ItemRegistry const&)            = delete;

public:
    MOD_NDAPI static ItemRegistry& getInstance();

    // True once the vanilla item pass has run, which is when items can be injected.
    MOD_NDAPI bool isReady() const noexcept;

    // Reserves the next item id, for an item that needs its final id *while it is being constructed* - a
    // `::BlockItem` takes state from the id it is handed, so a later id assignment leaves it inconsistent and
    // the engine lists a broken creative entry. Returns 0 before the item pass has run.

    // Registers the ready event emitter (idempotent); called by `ModAPI::load()` and by every
    // `DeferredRegister<ItemRegistry, ...>`.
    MOD_API static void ensureEventRegistered();

    template <class Entry>
    static constexpr bool isEntry = std::derived_from<Entry, ::Item>;

    // Builds `Entry` and injects it into the current item registry.
    template <std::derived_from<::Item> Entry, class... Args>
    ProductRef registerEntry(Args&&... args) {
        return registerItem<Entry>(std::forward<Args>(args)...);
    }

    template <std::derived_from<::Item> Entry, class... Args>
    ProductRef registerItem(Args&&... args) {
        return _registerItem([... args = std::forward<Args>(args)]() -> std::unique_ptr<::Item> {
            auto item = std::make_unique<Entry>(args...);
            if constexpr (requires { item->_init(); }) item->_init();
            return std::move(item);
        });
    }

    MOD_API void forEachItemInRegistry(std::function<bool(::Item& item)>&& func);

    MOD_NDAPI ::WeakPtr<::Item> getItem(std::string_view name);

public:
    MOD_API bool setFireResistant(std::string_view itemName, bool value = true);

    MOD_API bool addTag(std::string_view itemName, std::string_view tag);

    MOD_API bool setIcon(std::string_view itemName, std::string_view icon);

    MOD_API bool setDisplayName(std::string_view itemName, std::string_view displayName);

    MOD_API bool setRepairItem(std::string_view itemName, std::string_view fixItem);

    MOD_NDAPI ::CompoundTag& getAndModifyVanillaNetworkTagInfo(std::string_view itemName);

public:
    MOD_NDAPI ItemRegistry& _modifyItem(std::function<void(ItemRegistry&)>&&);

    // Builds the item and injects it; empty when the item pass has not run yet.
    MOD_NDAPI ProductRef _registerItem(std::function<std::unique_ptr<::Item>()>&& func);

    // The engine registered one of the items this registry handed to its pre-registry: the item now has its
    // id, so the work that needs it (command enum, creative entry) happens here.

    // Binds the registry vanilla is filling and publishes `ItemReadyEvent`.
    MOD_API void _bindRegistry(::ItemRegistry& registry);
};

} // namespace modapi::inline item

static_assert(modapi::Registry<modapi::ItemRegistry>);
