#pragma once
#include "modapi/Macros.h"
#include <mc/deps/core/utility/optional_ref.h>
#include <mc/world/level/BlockPos.h>
#include <mc/world/level/dimension/DimensionHeightRange.h>
#include <variant>

class Block;
class BlockSource;
class LevelChunk;

namespace modapi::inline worldgen {

// Handle to the region a world generation feature runs in.
//
// A feature is handed one through its `place()` call; which region it wraps depends on the
// generation step the engine called the feature in - a `BlockSource` for most of them, the
// `LevelChunk` itself while that chunk is generated.
//
// The handle is a lightweight value: copying or moving it refers to the same region (there is no
// allocation and nothing that a move can take away), and an empty handle - default constructed, or
// built from a null region - fails every operation instead of crashing. Nothing here throws; a
// rejected write is reported through the return value and the log.
class BlockHelper {
public:
    // A position holds two layers: the block itself, and the block stored *next to* it (water
    // logging, snow layers, ...).
    enum class Layer : uint8 {
        Block      = 0,
        ExtraBlock = 1,
    };

private:
    std::variant<std::monostate, BlockSource*, LevelChunk*> mRegion;

public:
    BlockHelper() = default;
    MOD_API explicit BlockHelper(BlockSource* source);
    MOD_API explicit BlockHelper(LevelChunk* chunk);
    ~BlockHelper() = default;

    BlockHelper(BlockHelper const&)            = default;
    BlockHelper(BlockHelper&&)                 = default;
    BlockHelper& operator=(BlockHelper const&) = default;
    BlockHelper& operator=(BlockHelper&&)      = default;

    // Whether the handle holds a region; a null region counts as empty.
    [[nodiscard]] MOD_API bool isValid() const noexcept;

    // Whether `pos` is inside the wrapped region and inside its dimension's height range.
    [[nodiscard]] MOD_API bool isValidPosition(BlockPos const& pos) const;

    // Writes `block` into `layer` at `pos`. Returns false - and logs an error - when the handle is
    // empty or `pos` is outside the region.
    MOD_API bool setBlock(BlockPos const& pos, Block const& block, Layer layer = Layer::Block, int updateFlags = 0);

    // Reads `layer` at `pos`; empty when the handle is empty or `pos` is outside the region.
    [[nodiscard]] MOD_API optional_ref<Block const> getBlock(BlockPos const& pos, Layer layer = Layer::Block) const;

    // The wrapped region's height range; a default range for an empty handle.
    [[nodiscard]] MOD_API DimensionHeightRange getHeightRange() const;
    [[nodiscard]] MOD_API short                getMinHeight() const;
    [[nodiscard]] MOD_API short                getMaxHeight() const;

    // The wrapped region: for a valid handle exactly one of the two getters is non null.
    [[nodiscard]] MOD_API BlockSource* getBlockSource() const noexcept;
    [[nodiscard]] MOD_API LevelChunk*  getLevelChunk() const noexcept;
    [[nodiscard]] MOD_API bool         holdsBlockSource() const noexcept;
    [[nodiscard]] MOD_API bool         holdsLevelChunk() const noexcept;
};

} // namespace modapi::inline worldgen
