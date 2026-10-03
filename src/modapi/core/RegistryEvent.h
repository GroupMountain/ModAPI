#pragma once
#include "modapi/core/Gloabl.h"
#include <atomic>
#include <concepts>
#include <ll/api/event/Emitter.h>
#include <ll/api/event/Event.h>
#include <ll/api/event/EventBus.h>
#include <ll/api/reflection/TypeName.h>
#include <memory>

namespace modapi::inline core {

// Registers the emitter for a registry ready event, once per process.
//
// The event has to be registered before anything can listen to it, and it has to be owned by
// ModAPI: LL drops every event owned by a mod when that mod is unloaded, so a consumer must not be
// the one registering it. That also means this cannot run from a DLL static initialiser - at that
// point the mod manager has not published ModAPI's own mod instance yet - so `ModAPI::load()` and
// every `DeferredRegister` construction call it instead.
template <std::derived_from<ll::event::Event> EventT>
void ensureRegistryEventRegistered(std::atomic_bool& registered) {
    if (registered.load(std::memory_order_acquire)) return;

    auto self = getSelfModPtr().lock();
    if (!self) {
        getLogger().error(
            "Failed to register the {} event: ModAPI's own mod instance is not available yet.",
            ll::reflection::type_stem_name_v<EventT>
        );
        return;
    }

    ll::event::EventBus::getInstance().setEventEmitter<EventT>(
        []() -> std::unique_ptr<ll::event::EmitterBase> { return std::make_unique<ll::event::EmitterBase>(); },
        self
    );
    registered.store(true, std::memory_order_release);
}

} // namespace modapi::inline core
