#pragma once
#include "modapi/Macros.h"
#include <concepts>
#include <ll/api/event/Event.h>
#include <mc/deps/core/utility/optional_ref.h>

namespace modapi {

// True once ModAPI is shutting down. During DLL teardown the event bus (and the rest of LeviLamina)
// may already be gone, so anything that destroys a listener has to skip that step then.
MOD_API bool isShuttingDown();

// A ModAPI registry announces the moment the vanilla registration pass it hooks has run by
// publishing `EventType`. Registries are singletons and store their `Product` in a handle that the
// registry APIs return (`ProductRef`, i.e. `optional_ref<Product>`).
template <class T>
concept Registry = requires(T& registry) {
    typename T::EventType; // final ll::event::Event subclass, published at the vanilla registration point
    typename T::Product;   // the object a successful registration produces (e.g. ::Item)
    typename T::ProductRef;

    requires std::derived_from<typename T::EventType, ll::event::Event>;
    requires std::same_as<typename T::ProductRef, optional_ref<typename T::Product>>;

    { registry.isReady() } -> std::convertible_to<bool>;
    { T::getInstance() } -> std::same_as<T&>;
    T::ensureEventRegistered();
};

// `Entry` is a user type the registry knows how to build and register.
template <class T, class Entry>
concept RegistryEntry = Registry<T> && T::template isEntry<Entry>;

} // namespace modapi
