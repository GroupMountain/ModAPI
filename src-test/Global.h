#pragma once

#include "Test.hpp" // IWYU pragma: export
#include <ll/api/event/EventBus.h>
#include <ll/api/event/server/ServerStartedEvent.h>
#include <string>

// Runs the function(s) when the server started, i.e. once every registry is open.
#define REGISTER_TEST(NAME, ...)                                                                                       \
    auto test_##NAME = [] {                                                                                            \
        registerTest(__VA_ARGS__);                                                                                     \
        return 0;                                                                                                      \
    }();

// Runs the function(s) while the test mod loads, i.e. before the server (and every registry) started.
#define REGISTER_ON_LOAD_TEST(NAME, ...)                                                                               \
    auto test_##NAME = [] {                                                                                            \
        test::Entry::getInstance().addOnLoadTest([] { __VA_ARGS__(); });                                               \
        return 0;                                                                                                      \
    }();

#define REGISTER_ON_UNLOAD_TEST(NAME, ...)                                                                             \
    auto test_##NAME = [] {                                                                                            \
        test::Entry::getInstance().addOnUnloadTest([] { __VA_ARGS__(); });                                             \
        return 0;                                                                                                      \
    }();

template <typename... Funcs>
void registerTest(Funcs... funcs) {
    test::listen<ll::event::ServerStartedEvent>([=](auto&) { (funcs(), ...); });
}

// Adds a `modapitest <name>` command entry that runs the suite and reports pass/fail.
void registerTestFunction(std::string const& name, std::function<bool()> const& callback);

void initTestCommand();
