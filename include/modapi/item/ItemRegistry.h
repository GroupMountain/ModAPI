#pragma once
#include "modapi/Macros.h"
#include "modapi/Registration.h"
#include "modapi/Registry.h"
#include "modapi/item/base/ICustomItem.h"
#include <concepts>
#include <functional>
#include <ll/api/event/Event.h>
#include <ll/api/service/Bedrock.h>
#include <mc/common/WeakPtr.h>
#include <mc/deps/core/utility/optional_ref.h>
#include <mc/world/item/Item.h>
#include <mc/world/level/Level.h>
#include <memory>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>

class ItemRegistry;

namespace modapi::inline item {

class ItemRegistry;

class ItemReadyEvent final : public ll::event::Event {
    ItemRegistry& mRegistry;

public:
    constexpr explicit ItemReadyEvent(ItemRegistry& registry) : mRegistry(registry) {}

    MOD_NDAPI ItemRegistry& registry() const;
};

template <class Entry>
concept CustomItemEntry =
    std::derived_from<Entry, ::Item> && requires { typename Entry::CustomItemBase; } && requires(Entry& entry) {
        { entry.getComponentItem() } -> std::convertible_to<::ComponentItem*>;
        entry.initComponentItem(std::declval<::cereal::ReflectionCtx const&>());
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

    MOD_NDAPI bool isReady() const noexcept;

    MOD_API static void ensureEventRegistered();

    template <class Entry>
    static constexpr bool isEntry = CustomItemEntry<Entry>;

    template <class Entry>
    static ClassRegistrationBuilder<ItemRegistry, Entry> builder(std::string identifier) {
        return ClassRegistrationBuilder<ItemRegistry, Entry>{std::move(identifier)};
    }

    template <CustomItemEntry Entry, class... Args>
    ProductRef registerEntry(ClassRegistration<Entry, Args...> registration) {
        return _registerItem(
            [identifier = registration.mIdentifier,
             arguments  = registration.mArguments]() mutable -> std::unique_ptr<::Item> {
                return std::apply(
                    [&identifier](auto&&... args) -> std::unique_ptr<::Item> {
                        return std::make_unique<Entry>(identifier, std::forward<decltype(args)>(args)...);
                    },
                    arguments
                );
            },
            [](::Item& item, ::cereal::ReflectionCtx const& ctx) { static_cast<Entry&>(item).initComponentItem(ctx); }
        );
    }

    MOD_API void forEachItemInRegistry(std::function<bool(::Item& item)>&& func);

    MOD_NDAPI ::WeakPtr<::Item> getItem(std::string_view name);

    // Builds the item, assigns its id, runs `afterId` and injects it; empty when the item pass has not run.
    MOD_NDAPI ProductRef _registerItem(
        std::function<std::unique_ptr<::Item>()>&&                     factory,
        std::function<void(::Item&, ::cereal::ReflectionCtx const&)>&& afterId = {}
    );

    MOD_API void _bindRegistry(::ItemRegistry& registry);
};

} // namespace modapi::inline item

static_assert(modapi::Registry<modapi::ItemRegistry>);
