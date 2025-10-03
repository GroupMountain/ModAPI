#pragma once
#include "modapi/Macros.h"
#include "modapi/worldgen/BlockHelper.h"
#include <mc/world/level/levelgen/feature/IFeature.h>
#include <optional>


class BlockPos;
class BlockSource;
class Random;
class LevelChunk;
namespace gmlib::mod {
class ICustomFeature : public IFeature {
public:
    MOD_API ICustomFeature();
    MOD_API virtual ~ICustomFeature();
    MOD_API virtual ::std::optional<::BlockPos> place(::IFeature::PlacementContext const&) const;
    /*must be thread-safe*/
    MOD_API virtual std::optional<BlockPos>
    place(BlockHelper& sourceOrChunk, const BlockPos& pos, Random& random) const = 0;
};
} // namespace gmlib::mod