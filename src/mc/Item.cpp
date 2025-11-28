#include <ll/api/memory/Memory.h>
#include <mc/deps/puv/puv_load_data/LoadResultWithTiming.h>
#include <mc/world/item/Item.h>

// 临时修复链接问题

PuvLoadData::LoadResultWithTiming Item::initServer(
    Json::Value const&  json,
    SemVersion const&   version,
    IPackLoadContext&   context,
    JsonBetaState const state
) {
    // clang-format off
    using namespace ll::memory_literals;
    using FuncType = PuvLoadData::LoadResultWithTiming (*)(
        Item*,
        Json::Value const&,
        SemVersion const&,
        IPackLoadContext&,
        JsonBetaState
    );
    static auto initFunc = reinterpret_cast<FuncType>(
        "48 89 5C 24 ?? 55 56 57 41 54 "
        "41 55 41 56 41 57 48 8D 6C 24 "
        "?? 48 81 EC ?? ?? ?? ?? 0F 29 "
        "B4 24 ?? ?? ?? ?? 48 8B 05 ?? "
        "?? ?? ?? 48 33 C4 48 89 45 ?? "
        "4D 8B F8 48 8B FA 48 8B F1"_sig.resolve()
    );
    return initFunc(this, json, version, context, state);
    // clang-format on
}

PuvLoadData::LoadResultWithTiming Item::initClient(
    Json::Value const&  json,
    SemVersion const&   version,
    JsonBetaState const state,
    IPackLoadContext&   context
) {
    // clang-format off
    using namespace ll::memory_literals;
    using FuncType = PuvLoadData::LoadResultWithTiming (*)(
        Item*,
        Json::Value const&,
        SemVersion const&,
        JsonBetaState const,
        IPackLoadContext&
    );
    static auto initFunc = reinterpret_cast<FuncType>(
        "48 89 5C 24 ?? 55 56 57 41 54 "
        "41 55 41 56 41 57 48 8D 6C 24 "
        "?? 48 81 EC ?? ?? ?? ?? 0F 29 "
        "B4 24 ?? ?? ?? ?? 48 8B 05 ?? "
        "?? ?? ?? 48 33 C4 48 89 45 ?? "
        "4D 8B F8 48 8B F2"_sig.resolve()
    );
    return initFunc(this, json, version, state, context);
    // clang-format on
}