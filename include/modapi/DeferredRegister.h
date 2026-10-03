#pragma once
#include "modapi/Registry.h"
#include <concepts>
#include <ll/api/event/EventBus.h>
#include <ll/api/event/Listener.h>
#include <ll/api/event/server/ServerStoppingEvent.h>
#include <ll/api/mod/NativeMod.h>
#include <ll/api/reflection/TypeName.h>
#include <memory>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>

namespace modapi {

namespace detail {

// Type erased registration request. The stored constructor arguments are re-used every time the
// registry announces a registration pass, so the entry is rebuilt for each pass (the vanilla
// registration points documented per registry can run more than once per process).
template <Registry T, class Entry>
class EntryInvoker {
public:
    virtual ~EntryInvoker() = default;

    virtual typename T::ProductRef invoke(T& registry) = 0;
};

template <Registry T, class Entry, class... Args>
class EntryInvokerImpl : public EntryInvoker<T, Entry> {
    std::tuple<Args...> mArgs;

public:
    explicit EntryInvokerImpl(Args... args) : mArgs(std::move(args)...) {}

    typename T::ProductRef invoke(T& registry) override {
        return std::apply(
            [&registry](auto&... args) ->
            typename T::ProductRef { return registry.template registerEntry<Entry>(args...); },
            mArgs
        );
    }
};

} // namespace detail

// Deferred registration of one user defined type into one registry.
//
// The constructor does two things: it stores the arguments used to build `Entry`, and it attaches a
// listener for `T::EventType`. There is no retry and no state machine: whenever the registry
// publishes its ready event, the entry is built from the stored arguments and registered, and the
// produced object is kept for `get()`.
//
//   class MyItem : public modapi::ICustomItem { ... };
//   static modapi::DeferredRegister<modapi::ItemRegistry, MyItem> gMyItem("mymod:my_item", 100);
//
// The object must be static/global and constructed before the first registration pass of `T`;
// afterwards the ready event it waits for has already been published and it never runs.
template <Registry T, class Entry>
    requires RegistryEntry<T, Entry>
class DeferredRegister {
public:
    using RegistryT  = T;
    using EntryT     = Entry;
    using Product    = typename T::Product;
    using ProductRef = typename T::ProductRef;
    using EventType  = typename T::EventType;

    // The arguments are stored and reused, so they have to be copyable.
    template <class... Args>
        requires(std::copy_constructible<std::decay_t<Args>> && ...)
    explicit DeferredRegister(Args&&... args)
    : mState(
          std::make_shared<State>(
              std::make_unique<detail::EntryInvokerImpl<T, Entry, std::decay_t<Args>...>>(std::forward<Args>(args)...)
          )
      ) {
        T::ensureEventRegistered();
        auto& bus = ll::event::EventBus::getInstance();
        mListener = bus.emplaceListener<EventType>([state = std::weak_ptr<State>(mState)](EventType& event) {
            if (auto locked = state.lock()) locked->registerEntry(event.registry());
        });
        if (!mListener) {
            logError("could not listen for the registry ready event (is the event registered?)");
        }
        // The ready event listener holds a lambda whose code lives in the module that built this
        // object, so it must not stay in the bus after that module is unloaded. `ServerStoppingEvent`
        // is the last moment at which both are still loaded, so the registration detaches itself
        // there - long before the DLL teardown that a static destructor would run in.
        mStopListener = bus.emplaceListener<ll::event::server::ServerStoppingEvent>(
            [this](ll::event::server::ServerStoppingEvent&) { detach(); }
        );
    }

    ~DeferredRegister() {
        detach();
        if (!mState->mRegistered && !modapi::isShuttingDown()) {
            logError("the deferred registration was never applied (constructed after the registry opened?)");
        }
    }

    // Removes the listeners this object attached. Called by the destructor and when the server stops;
    // doing it twice is harmless.
    void detach() {
        auto& bus = ll::event::EventBus::getInstance();
        if (mListener) {
            bus.removeListener<EventType>(mListener);
            mListener = nullptr;
        }
        if (mStopListener) {
            bus.removeListener<ll::event::server::ServerStoppingEvent>(mStopListener);
            mStopListener = nullptr;
        }
    }

    DeferredRegister(DeferredRegister const&)            = delete;
    DeferredRegister(DeferredRegister&&)                 = delete;
    DeferredRegister& operator=(DeferredRegister const&) = delete;
    DeferredRegister& operator=(DeferredRegister&&)      = delete;

    // Whether the entry has been registered at least once.
    [[nodiscard]] bool isRegistered() const noexcept { return mState->mRegistered; }

    // Whether the target registry is open (forwards to the registry's `isReady()`).
    [[nodiscard]] bool isReady() const noexcept { return T::getInstance().isReady(); }

    // The object produced by the most recent registration. Empty until `isRegistered()`, and also
    // empty for registrations that do not produce a single object (furnace and brewing recipes).
    [[nodiscard]] ProductRef get() const noexcept { return mState->mProduct; }

    Product& operator*() const { return *get(); }

    Product* operator->() const { return get().as_ptr(); }

private:
    struct State {
        std::unique_ptr<detail::EntryInvoker<T, Entry>> mInvoker;
        ProductRef                                      mProduct{};
        bool                                            mRegistered = false;

        explicit State(std::unique_ptr<detail::EntryInvoker<T, Entry>> invoker) : mInvoker(std::move(invoker)) {}

        void registerEntry(T& registry) {
            if (!mInvoker) return;
            try {
                mProduct    = mInvoker->invoke(registry);
                mRegistered = true;
            } catch (...) {
                // A failing registration must not escape into the event publisher.
            }
        }
    };

    std::shared_ptr<State> mState;
    ll::event::ListenerPtr mListener;
    ll::event::ListenerPtr mStopListener;

    static void logError(std::string_view message) {
        if (auto mod = ll::mod::NativeMod::current()) {
            mod->getLogger().error(
                "DeferredRegister<{}, {}>: {}",
                ll::reflection::type_stem_name_v<T>,
                ll::reflection::type_stem_name_v<Entry>,
                message
            );
        }
    }
};

} // namespace modapi
