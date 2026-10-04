#include "modapi/worldgen/BlockHelper.h"
#include "modapi/core/Gloabl.h"
#include <mc/world/level/BlockSource.h>
#include <mc/world/level/ChunkBlockPos.h>
#include <mc/world/level/block/BlockChangeContext.h>
#include <mc/world/level/chunk/LevelChunk.h>
#include <mc/world/level/dimension/Dimension.h>

namespace modapi::inline worldgen {

namespace {

constexpr int ChunkSize = 16;

bool inHeightRange(DimensionHeightRange const& range, BlockPos const& pos) {
    return pos.y < range.mMax && pos.y >= range.mMin;
}

bool checkPosition(BlockSource const* source, BlockPos const& pos) {
    return source->hasChunksAt(pos, 0, false) && inHeightRange(source->mDimension.mHeightRange, pos);
}

// The local coordinate has to be *inside* the chunk: comparing only the high edge would accept the position before
// the chunk - `origin - 1` - which then wraps into the neighbouring chunk.
bool checkPosition(LevelChunk const* chunk, BlockPos const& pos) {
    auto const chunkPos = *chunk->mPosition;
    auto const localX   = pos.x - chunkPos.x * ChunkSize;
    auto const localZ   = pos.z - chunkPos.z * ChunkSize;
    return localX >= 0 && localX < ChunkSize && localZ >= 0 && localZ < ChunkSize
        && inHeightRange(chunk->mDimension.mHeightRange, pos);
}

bool writeBlock(
    BlockSource*       source,
    BlockPos const&    pos,
    Block const&       block,
    BlockHelper::Layer layer,
    int                updateFlags
) {
    if (!checkPosition(source, pos)) return false;
    if (layer == BlockHelper::Layer::Block) {
        source->setBlock(pos, block, updateFlags, nullptr, {});
        return true;
    }
    if (layer == BlockHelper::Layer::ExtraBlock) {
        source->setExtraBlock(pos, block, updateFlags);
        return true;
    }
    return false; // an out of range `Layer` value (only reachable through a cast)
}

bool writeBlock(LevelChunk* chunk, BlockPos const& pos, Block const& block, BlockHelper::Layer layer, int) {
    if (!checkPosition(chunk, pos)) return false;
    auto const chunkPos = ChunkBlockPos{pos, chunk->mDimension.mHeightRange->mMin};
    if (layer == BlockHelper::Layer::Block) {
        chunk->setBlock(chunkPos, block, nullptr, nullptr, {});
        return true;
    }
    if (layer == BlockHelper::Layer::ExtraBlock) {
        chunk->setExtraBlock(chunkPos, block, nullptr);
        return true;
    }
    return false;
}

optional_ref<Block const> readBlock(BlockSource const* source, BlockPos const& pos, BlockHelper::Layer layer) {
    if (!checkPosition(source, pos)) return std::nullopt;
    if (layer == BlockHelper::Layer::Block) return source->getBlock(pos);
    if (layer == BlockHelper::Layer::ExtraBlock) return source->getExtraBlock(pos);
    return std::nullopt;
}

optional_ref<Block const> readBlock(LevelChunk const* chunk, BlockPos const& pos, BlockHelper::Layer layer) {
    if (!checkPosition(chunk, pos)) return std::nullopt;
    auto const chunkPos = ChunkBlockPos{pos, chunk->mDimension.mHeightRange->mMin};
    if (layer == BlockHelper::Layer::Block) return chunk->getBlock(chunkPos);
    if (layer == BlockHelper::Layer::ExtraBlock) {
        return chunk->mDimension.getBlockSourceFromMainChunkSource().getExtraBlock(pos);
    }
    return std::nullopt;
}

} // namespace

BlockHelper::BlockHelper(BlockSource* source) : mRegion(source) {}
BlockHelper::BlockHelper(LevelChunk* chunk) : mRegion(chunk) {}

bool BlockHelper::isValid() const noexcept {
    return std::visit(
        [](auto value) -> bool {
            using Region = std::decay_t<decltype(value)>;
            if constexpr (std::is_same_v<Region, std::monostate>) {
                return false;
            } else {
                return value != nullptr;
            }
        },
        mRegion
    );
}

bool BlockHelper::isValidPosition(BlockPos const& pos) const {
    return std::visit(
        [&pos](auto value) -> bool {
            using Region = std::decay_t<decltype(value)>;
            if constexpr (std::is_same_v<Region, std::monostate>) {
                return false;
            } else {
                return value != nullptr && checkPosition(value, pos);
            }
        },
        mRegion
    );
}

bool BlockHelper::setBlock(BlockPos const& pos, Block const& block, Layer layer, int updateFlags) {
    auto const written = std::visit(
        [&](auto value) -> bool {
            using Region = std::decay_t<decltype(value)>;
            if constexpr (std::is_same_v<Region, std::monostate>) {
                return false;
            } else {
                return value != nullptr && writeBlock(value, pos, block, layer, updateFlags);
            }
        },
        mRegion
    );
    if (!written) {
        core::getLogger().error(
            "BlockHelper: nothing was written at ({}, {}, {}) - the region is empty or the position is outside it",
            pos.x,
            pos.y,
            pos.z
        );
    }
    return written;
}

optional_ref<Block const> BlockHelper::getBlock(BlockPos const& pos, Layer layer) const {
    return std::visit(
        [&](auto value) -> optional_ref<Block const> {
            using Region = std::decay_t<decltype(value)>;
            if constexpr (std::is_same_v<Region, std::monostate>) {
                return std::nullopt;
            } else {
                if (value == nullptr) return std::nullopt;
                return readBlock(value, pos, layer);
            }
        },
        mRegion
    );
}

DimensionHeightRange BlockHelper::getHeightRange() const {
    return std::visit(
        [](auto value) -> DimensionHeightRange {
            using Region = std::decay_t<decltype(value)>;
            if constexpr (std::is_same_v<Region, std::monostate>) {
                return {};
            } else {
                if (value == nullptr) return {};
                return value->mDimension.mHeightRange;
            }
        },
        mRegion
    );
}

short BlockHelper::getMinHeight() const { return getHeightRange().mMin; }

short BlockHelper::getMaxHeight() const { return getHeightRange().mMax; }

BlockSource* BlockHelper::getBlockSource() const noexcept {
    if (auto const* source = std::get_if<BlockSource*>(&mRegion)) return *source;
    return nullptr;
}

LevelChunk* BlockHelper::getLevelChunk() const noexcept {
    if (auto const* chunk = std::get_if<LevelChunk*>(&mRegion)) return *chunk;
    return nullptr;
}

bool BlockHelper::holdsBlockSource() const noexcept { return getBlockSource() != nullptr; }

bool BlockHelper::holdsLevelChunk() const noexcept { return getLevelChunk() != nullptr; }

} // namespace modapi::inline worldgen
