#include "Global.h"
#include "Test.hpp"
#include <ll/api/mod/RegisterHelper.h>

namespace test {

namespace {
// Everything a suite attached to the event bus, so `Entry::unload()` can take it off again.
std::vector<std::function<void()>> gListenerRemovers;
} // namespace

void listenRemovable(std::function<void()> remove) { gListenerRemovers.push_back(std::move(remove)); }

void removeListeners() {
    for (auto& remove : gListenerRemovers) {
        try {
            remove();
        } catch (...) {}
    }
    gListenerRemovers.clear();
}

ll::io::Logger& getLogger() { return Entry::getInstance().getSelf().getLogger(); }

Entry& Entry::getInstance() {
    static Entry instance;
    return instance;
}

bool Entry::load() {
    for (auto& test : mPendingOnLoadTests) {
        try {
            test();
        } catch (...) {}
    }
    return true;
}

bool Entry::enable() { return true; }

bool Entry::disable() { return true; }

bool Entry::unload() {
    for (auto& test : mPendingOnUnloadTests) {
        try {
            test();
        } catch (...) {}
    }
    // Everything this mod attached to the event bus has to go before the DLL does, or the bus would
    // call a lambda whose code lives in an unloaded module while the process shuts down.
    removeListeners();
    return true;
}

void Entry::addOnLoadTest(std::function<void()>&& func) { mPendingOnLoadTests.push_back(std::move(func)); }

void Entry::addOnUnloadTest(std::function<void()>&& func) { mPendingOnUnloadTests.push_back(std::move(func)); }

} // namespace test

LL_REGISTER_MOD(test::Entry, test::Entry::getInstance());
