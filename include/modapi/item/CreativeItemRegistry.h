#pragma once
#include "modapi/Macros.h"
#include "modapi/Registration.h"
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

struct CreativeGroupRegistration {
    std::string                         mGroup;
    ::ItemInstance                      mIcon;
    ::SharedTypes::CreativeItemCategory mCategory = ::SharedTypes::CreativeItemCategory::Items;
};

class CreativeGroupRegistrationBuilder {
    CreativeGroupRegistration mRegistration;

public:
    explicit CreativeGroupRegistrationBuilder(std::string group) { mRegistration.mGroup = std::move(group); }

    CreativeGroupRegistrationBuilder& category(::SharedTypes::CreativeItemCategory value) {
        mRegistration.mCategory = value;
        return *this;
    }

    CreativeGroupRegistrationBuilder& icon(::ItemInstance value) {
        mRegistration.mIcon = std::move(value);
        return *this;
    }

    [[nodiscard]] DataRegistration<CreativeGroupRegistration> build() const { return {mRegistration}; }
};

class CreativeItemRegistry {
    struct Impl;
    std::unique_ptr<Impl> pImpl;

public:
    using EventType  = CreativeItemReadyEvent;
    using Product    = ::CreativeItemEntry;
    using ProductRef = optional_ref<::CreativeItemEntry>;
    using Property   = CreativeGroupRegistration;
    using DataEntry  = CreativeGroupRegistration;

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

    template <class Entry>
    static constexpr bool isEntry = std::same_as<Entry, CreativeGroupRegistration>;

    static CreativeGroupRegistrationBuilder builder(std::string group) {
        return CreativeGroupRegistrationBuilder{std::move(group)};
    }

    static CreativeGroupRegistration makeDataEntry(std::string group, CreativeGroupRegistration registration) {
        registration.mGroup = std::move(group);
        return registration;
    }

    ProductRef registerEntry(DataRegistration<CreativeGroupRegistration> registration) {
        registerCreativeGroup(
            registration.mEntry.mGroup,
            std::move(registration.mEntry.mIcon),
            registration.mEntry.mCategory
        );
        return {};
    }

public:
    MOD_API uint32_t registerCreativeGroup(
        std::string_view                    groupName,
        ::ItemInstance&&                    icon     = ::ItemInstance(),
        ::SharedTypes::CreativeItemCategory category = ::SharedTypes::CreativeItemCategory::Items
    );

    // Binds the instance vanilla is filling and publishes `CreativeItemReadyEvent` for it.
    MOD_API void _bindRegistry(::CreativeItemRegistry& registry);
};

} // namespace modapi::inline item

static_assert(modapi::Registry<modapi::CreativeItemRegistry>);
