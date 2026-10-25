#pragma once
#include <mc/world/level/LevelSeed64.h>
#include <mc/world/level/biome/surface/ISurfaceBuilder.h>

namespace modapi::inline worldgen {

// The base a surface builder written in C++ derives from.
//
// The engine picks builders with `isBestBuilder`, which only sees the biome's `SurfaceBuilderData`
// and cannot tell which biome it belongs to. ModAPI therefore never lets a custom builder take part
// in that resolution: it installs the builder for the biomes that name it, per chunk, right before
// `buildChunkSurface` runs. Only `buildSurfaceAt` has to be implemented here.
//
// Writing blocks: `column.mBlocks` is the whole chunk's staging volume (16 x 320 x 16), laid out
// y-first - `index = localY + mHeight * (localZ + localX * mDepth)` with `localY = worldY -
// mDimensionBottom`, which is how the engine's own `BlockVolumeTarget::setBlock` writes it. The
// engine commits the volume into the chunk once the builder returns, so a write here reaches the
// world; reading the volume back during the call tells nothing.
//
// Every member is defined in the class, so a mod derives from it without needing a symbol from
// ModAPI.
class ICustomSurfaceBuilder : public ::ISurfaceBuilder {
public:
    // What a column looks like: `column.mBlocks` is the volume to write into, `column.mPos` the
    // column's position (`y` is always 0; only `x`/`z` are meaningful), `column.mDepthValue` its
    // depth and `column.mHeightRange` the range the surface may occupy.
    void initBuilder(::LevelSeed64) override {}

    void initBiomeSurface(::SurfaceBuilderData&) const override {}

    // A custom builder is never selected by the engine's own resolution; ModAPI installs it for the
    // biomes that name it.
    bool isBestBuilder(::SurfaceBuilderData const&) const override { return false; }

    bool getUsesDepthValue() const override { return false; }
};

} // namespace modapi::inline worldgen
