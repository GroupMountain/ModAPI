#pragma once
#include "modapi/Macros.h"
#include "modapi/Registration.h"
#include "modapi/Registry.h"
#include "modapi/worldgen/base/ICustomSurfaceBuilder.h"
#include <ll/api/event/Event.h>
#include <mc/deps/core/utility/optional_ref.h>
#include <memory>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>

class SurfaceBuilderRegistry;

namespace modapi::inline worldgen {

class SurfaceBuilderRegistry;

class SurfaceBuilderReadyEvent final : public ll::event::Event {
    SurfaceBuilderRegistry& mRegistry;

public:
    constexpr explicit SurfaceBuilderReadyEvent(SurfaceBuilderRegistry& registry) : mRegistry(registry) {}

    MOD_API SurfaceBuilderRegistry& registry() const;
};

class SurfaceBuilderRegistry {
public:
    struct Impl;
    std::unique_ptr<Impl> pImpl;

public:
    using EventType  = SurfaceBuilderReadyEvent;
    using Product    = ::ISurfaceBuilder;
    using ProductRef = optional_ref<::ISurfaceBuilder>;

public:
    SurfaceBuilderRegistry();
    SurfaceBuilderRegistry& operator=(SurfaceBuilderRegistry const&) = delete;
    SurfaceBuilderRegistry(SurfaceBuilderRegistry const&)            = delete;
    ~SurfaceBuilderRegistry();

public:
    MOD_NDAPI static SurfaceBuilderRegistry& getInstance();

    [[nodiscard]] MOD_NDAPI bool isReady() const noexcept;

    MOD_API static void ensureEventRegistered();

    template <class Entry>
    static constexpr bool isEntry = std::derived_from<Entry, ICustomSurfaceBuilder>;

    template <class Entry>
    static ClassRegistrationBuilder<SurfaceBuilderRegistry, Entry> builder(std::string identifier) {
        return ClassRegistrationBuilder<SurfaceBuilderRegistry, Entry>{std::move(identifier)};
    }

    template <class Entry, class... Args>
        requires isEntry<Entry>
    ProductRef registerEntry(ClassRegistration<Entry, Args...> registration) {
        try {
            auto builder = std::apply(
                [](auto&&... args) -> std::unique_ptr<::ISurfaceBuilder> {
                    return std::make_unique<Entry>(::std::forward<decltype(args)>(args)...);
                },
                registration.mArguments
            );
            return _registerBuilder(registration.mIdentifier, ::std::move(builder));
        } catch (...) {
            return {};
        }
    }

    [[nodiscard]] MOD_NDAPI ProductRef getBuilder(std::string_view identifier) const;

    MOD_API void _bindRegistry(::SurfaceBuilderRegistry& registry);

    MOD_NDAPI ProductRef _registerBuilder(std::string_view identifier, std::unique_ptr<::ISurfaceBuilder> builder);
};

} // namespace modapi::inline worldgen

static_assert(modapi::Registry<modapi::SurfaceBuilderRegistry>);
