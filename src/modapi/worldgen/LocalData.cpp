#include "modapi/worldgen/LocalData.h"
#include <ll/api/memory/Hook.h>
#include <mc/world/level/biome/components/BiomeDecorationFeature.h>
#include <mc/world/level/biome/systems/BiomeDecorationSystem.h>
namespace modapi::inline worldgen {
LL_STATIC_HOOK(
    BiomeDecorationSystemDecorateBiomeHook,
    HookPriority::Normal,
    &::BiomeDecorationSystem::decorateBiome,
    bool,
    ::LevelChunk&                               lc,
    ::BlockSource&                              source,
    ::Random&                                   random,
    ::gsl::span<::BiomeDecorationFeature const> featureList,
    ::std::string const&                        pass,
    ::Biome const*                              biome,
    ::IPreliminarySurfaceProvider const&        preliminarySurfaceProvider
) {
    modapi::LocalData::getInstance().mBlockSource = &source;
    modapi::LocalData::getInstance().mRandom      = &random;
    modapi::LocalData::getInstance().mLevelChunk  = nullptr;
    return origin(lc, source, random, featureList, pass, biome, preliminarySurfaceProvider);
}

LL_STATIC_HOOK(
    BiomeDecorationSystemDecorateLargeFeatureHook,
    HookPriority::Normal,
    &::BiomeDecorationSystem::decorateLargeFeature,
    void,
    ::Biome const&       biome,
    ::LevelChunk&        lc,
    ::BlockVolumeTarget& target,
    ::Random&            random,
    ::ChunkPos const&    pos,
    ::std::string const& pass
) {
    modapi::LocalData::getInstance().mBlockSource = nullptr;
    modapi::LocalData::getInstance().mRandom      = &random;
    modapi::LocalData::getInstance().mLevelChunk  = &lc;
    origin(biome, lc, target, random, pos, pass);
}
void LocalData::init() {
    static auto hook = std::make_unique<ll::memory::HookRegistrar<
        BiomeDecorationSystemDecorateBiomeHook,
        BiomeDecorationSystemDecorateLargeFeatureHook>>();
}
LocalData& LocalData::getInstance() {
    init();
    static thread_local LocalData instance;
    return instance;
}
} // namespace modapi::inline worldgen