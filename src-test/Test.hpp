#pragma once
#include <ll/api/event/EventBus.h>
#include <ll/api/io/Logger.h>
#include <ll/api/mod/NativeMod.h>
#include <functional>
#include <utility>
#include <vector>

namespace test {

// The test mod itself: a normal LeviLamina native mod whose callbacks run the registered tests.
class Entry {
public:
    static Entry& getInstance();

    Entry() : mSelf(*ll::mod::NativeMod::current()) {}

    [[nodiscard]] ll::mod::NativeMod& getSelf() const { return mSelf; }

    bool load();

    bool enable();

    bool disable();

    bool unload();

    void addOnLoadTest(std::function<void()>&& func);

    void addOnUnloadTest(std::function<void()>&& func);

private:
    ll::mod::NativeMod&                mSelf;
    std::vector<std::function<void()>> mPendingOnLoadTests;
    std::vector<std::function<void()>> mPendingOnUnloadTests;
};

ll::io::Logger& getLogger();

// Attaches a listener the mod can remove again. A lambda's code lives in this DLL, so leaving it in
// the event bus after unload makes the bus call into an unloaded module.
template <class Event, class Fn>
void listen(Fn&& fn) {
    auto listener = ll::event::EventBus::getInstance().emplaceListener<Event>(std::forward<Fn>(fn));
    listenRemovable([listener] { ll::event::EventBus::getInstance().removeListener<Event>(listener); });
}

void listenRemovable(std::function<void()> remove);

void removeListeners();

} // namespace test
