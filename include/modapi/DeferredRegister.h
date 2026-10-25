#pragma once
#include "modapi/Registration.h"
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

template <Registry T, class Entry>
class EntryInvoker {
public:
    virtual ~EntryInvoker() = default;

    virtual typename T::ProductRef invoke(T& registry) = 0;
};

template <Registry T, class Entry, class... Args>
class ClassEntryInvoker : public EntryInvoker<T, Entry> {
    ClassRegistration<Entry, Args...> mRegistration;

public:
    explicit ClassEntryInvoker(ClassRegistration<Entry, Args...> registration)
    : mRegistration(std::move(registration)) {}

    typename T::ProductRef invoke(T& registry) override {
        return registry.template registerEntry<Entry>(mRegistration);
    }
};

template <Registry T, class Entry>
class DataEntryInvoker : public EntryInvoker<T, Entry> {
    DataRegistration<Entry> mRegistration;

public:
    explicit DataEntryInvoker(DataRegistration<Entry> registration) : mRegistration(std::move(registration)) {}

    typename T::ProductRef invoke(T& registry) override { return registry.registerEntry(mRegistration); }
};

} // namespace detail

// Deferred registration of one entry into one registry. The registration object is stored and replayed
// for every ready event, so it must be copyable. There is no retry and no state machine.
template <Registry T, class Entry>
    requires RegistryEntry<T, Entry>
class DeferredRegister {
public:
    using RegistryT  = T;
    using EntryT     = Entry;
    using Product    = typename T::Product;
    using ProductRef = typename T::ProductRef;
    using EventType  = typename T::EventType;

    template <class... Args>
    explicit DeferredRegister(ClassRegistration<Entry, Args...> registration)
    : mState(
          std::make_shared<State>(
              std::make_unique<detail::ClassEntryInvoker<T, Entry, Args...>>(std::move(registration))
          )
      ) {
        attach();
    }

    explicit DeferredRegister(DataRegistration<Entry> registration)
    : mState(std::make_shared<State>(std::make_unique<detail::DataEntryInvoker<T, Entry>>(std::move(registration)))) {
        attach();
    }

    ~DeferredRegister() {
        detach();
        if (!mState->mRegistered && !modapi::isShuttingDown()) {
            logError("the deferred registration was never applied (constructed after the registry opened?)");
        }
    }

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

    [[nodiscard]] bool isRegistered() const noexcept { return mState->mRegistered; }

    [[nodiscard]] bool isReady() const noexcept { return T::getInstance().isReady(); }

    [[nodiscard]] ProductRef get() const noexcept { return mState->mProduct; }

private:
    void attach() {
        T::ensureEventRegistered();
        auto& bus = ll::event::EventBus::getInstance();
        mListener = bus.emplaceListener<EventType>([state = std::weak_ptr<State>(mState)](EventType& event) {
            if (auto locked = state.lock()) locked->registerEntry(event.registry());
        });
        if (!mListener) {
            logError("could not listen for the registry ready event (is the event registered?)");
        }
        mStopListener = bus.emplaceListener<ll::event::server::ServerStoppingEvent>(
            [this](ll::event::server::ServerStoppingEvent&) { detach(); }
        );
    }

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
            } catch (...) {}
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
