#include "modapi/core/ModApi.h"
#include "modapi/core/Gloabl.h"
#include <ll/api/mod/RegisterHelper.h>
#include <ll/api/service/GamingStatus.h>

#include <ll/api/thread/ThreadPoolExecutor.h>
#include <ll/api/coro/CoroTask.h>

namespace modapi {

ModApi& ModApi::getInstance() {
    static ModApi instance;
    return instance;
}

bool ModApi::load() { return true; }

bool ModApi::enable() {
    ll::thread::ThreadPoolExecutor mThreadPool{"TestThreadPool"};
    // ll::coro::keepThis([]() -> ll::coro::CoroTask<> {
    //     for (size_t count = 10; count > 0; --count) {
    //         co_await std::chrono::seconds(1);
    //         getLogger().info("Test ThreadPool: {}", count);
    //     }
    // }).syncLaunch(mThreadPool);
    auto task = []() -> ll::coro::CoroTask<> {
        for (size_t count = 10; count > 0; --count) {
            getLogger().info("Test ThreadPool: {}", count);
            co_await std::chrono::seconds(1);
        }
        co_return;
    }();
    std::this_thread::sleep_for(std::chrono::seconds(3));
    task.launch(mThreadPool);

    return true;
}

bool ModApi::disable() {
    if (ll::getGamingStatus() != ll::GamingStatus::Stopping) {
        getLogger().error("It is prohibited to disable ModApi mod when the server is not stopped.");
        return false;
    }
    return true;
}

ModApi&         getInstance() { return ModApi::getInstance(); }
ll::mod::Mod&   getSelfMod() { return getInstance().getSelf(); }
ll::io::Logger& getLogger() { return getSelfMod().getLogger(); }

} // namespace modapi

LL_REGISTER_MOD(modapi::ModApi, modapi::ModApi::getInstance());