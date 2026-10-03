#pragma once
#include "modapi/Macros.h"
#include "modapi/Registry.h"
#include <ll/api/event/Event.h>
#include <mc/deps/core/utility/optional_ref.h>
#include <mc/deps/shared_types/item/CreativeItemCategory.h>
#include <mc/world/item/ItemInstance.h>
#include <mc/world/item/registry/CreativeItemEntry.h>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace modapi::inline item {

class CreativeItemRegistry;

// Published right before vanilla rebuilds the creative item network ids, i.e. after
// `VanillaItems::serverInitCreativeItemsCallback` filled the registry and before
// `CreativeItemRegistry::updateNetIdMap` runs. Vanilla builds a new CreativeItemRegistry for every
// pass, so this event is published once per pass and carries the instance to register into.
class CreativeItemReadyEvent final : public ll::event::Event {
    CreativeItemRegistry& mRegistry;

public:
    constexpr explicit CreativeItemReadyEvent(CreativeItemRegistry& registry) : mRegistry(registry) {}

    MOD_NDAPI CreativeItemRegistry& registry() const;
};

class CreativeItemRegistry {
    struct Impl;
    std::unique_ptr<Impl> pImpl;

public:
    struct CreativeItem {
        ::ItemInstance&                     mItem;
        ::SharedTypes::CreativeItemCategory mCategory;
        std::string                         mGroup;
    };

public:
    using EventType  = CreativeItemReadyEvent;
    using Product    = ::CreativeItemEntry;
    using ProductRef = optional_ref<::CreativeItemEntry>;

public:
    CreativeItemRegistry();
    CreativeItemRegistry& operator=(CreativeItemRegistry const&) = delete;
    CreativeItemRegistry(CreativeItemRegistry const&)            = delete;

public:
    MOD_NDAPI static CreativeItemRegistry& getInstance();

    // True once a creative item registry instance has been seen.
    MOD_NDAPI bool isReady() const noexcept;

    // Registers the ready event emitter (idempotent).
    MOD_API static void ensureEventRegistered();

public:
    MOD_API uint32_t registerCreativeGroup(
        std::string_view                    groupName,
        ::ItemInstance&&                    icon     = ::ItemInstance(),
        ::SharedTypes::CreativeItemCategory category = ::SharedTypes::CreativeItemCategory::Items
    );

    MOD_NDAPI ProductRef registerCreativeItem(
        ::ItemInstance&&                    item,
        ::SharedTypes::CreativeItemCategory category  = ::SharedTypes::CreativeItemCategory::Items,
        std::string_view                    itemGroup = {}
    );

    MOD_NDAPI std::vector<CreativeItem> getCreativeItem(std::string_view itemType);

    MOD_API bool unregisterCreativeItem(CreativeItem const& item);

    // Binds the instance vanilla is filling and publishes `CreativeItemReadyEvent` for it.
    MOD_API void _bindRegistry(::CreativeItemRegistry& registry);

private:
    // Registers into the currently bound registry.
    MOD_NDAPI ProductRef _appendCreativeItem(
        ::ItemInstance&&                    item,
        ::SharedTypes::CreativeItemCategory category,
        std::string_view                    itemGroup
    );
};

} // namespace modapi::inline item

static_assert(modapi::Registry<modapi::CreativeItemRegistry>);
