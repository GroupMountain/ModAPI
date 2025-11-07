#include "modapi/worldgen/BlockHelper.h"
#include <mc/world/level/BlockSource.h>
#include <mc/world/level/ChunkBlockPos.h>
#include <mc/world/level/block/BlockChangeContext.h>
#include <mc/world/level/chunk/LevelChunk.h>
#include <mc/world/level/dimension/Dimension.h>

namespace modapi::inline worldgen {

namespace block_helper::details {
bool check(BlockSource const* source, BlockPos const& pos) {
    return source->hasChunksAt(pos, 0, false) && pos.y < source->mDimension.mHeightRange->mMax
        && pos.y >= source->mDimension.mHeightRange->mMin;
}
bool check(LevelChunk const* chunk, BlockPos const& pos) {
    return pos.x - chunk->mPosition->x * 16 < 16 && pos.z - chunk->mPosition->z * 16 < 16
        && pos.y < chunk->mDimension.mHeightRange->mMax && pos.y >= chunk->mDimension.mHeightRange->mMin;
}
void setBlock(BlockSource* source, BlockPos const& pos, Block const& block, uchar layer, int updateFlags) {
    if (!check(source, pos)) throw std::runtime_error("invalid position.");
    switch (layer) {
    case 0:
        source->setBlock(pos, block, updateFlags, nullptr, {});
        break;
    case 1:
        source->setExtraBlock(pos, block, updateFlags);
        break;
    default:
        throw std::out_of_range("layer out of range.");
    }
}
void setBlock(LevelChunk* chunk, BlockPos const& pos, Block const& block, uchar layer, int) {
    if (!check(chunk, pos)) throw std::runtime_error("invalid position.");
    const auto chunkPos = ChunkBlockPos{pos, chunk->mDimension.mHeightRange->mMin};
    switch (layer) {
    case 0:
        chunk->setBlock(chunkPos, block, nullptr, nullptr, {});
        break;
    case 1:
        chunk->setExtraBlock(chunkPos, block, nullptr);
        break;
    default:
        throw std::out_of_range("layer out of range.");
    }
}
Block const& getBlock(BlockSource const* source, BlockPos const& pos, uchar layer) {
    if (!check(source, pos)) throw std::runtime_error("invalid position.");
    switch (layer) {
    case 0:
        return source->getBlock(pos);
    case 1:
        return source->getExtraBlock(pos);
    default:
        throw std::out_of_range("layer out of range.");
    }
}
Block const& getBlock(LevelChunk const* chunk, BlockPos const& pos, uchar layer) {
    if (!check(chunk, pos)) throw std::runtime_error("invalid position.");
    const auto chunkPos = ChunkBlockPos{pos, chunk->mDimension.mHeightRange->mMin};
    switch (layer) {
    case 0:
        return chunk->getBlock(chunkPos);
    case 1:
        return chunk->getExtraBlock(chunkPos);
    default:
        throw std::out_of_range("layer out of range.");
    }
}
} // namespace block_helper::details
struct BlockHelper::Impl : std::variant<BlockSource*, LevelChunk*> {
    using std::variant<BlockSource*, LevelChunk*>::variant;
};
BlockHelper::BlockHelper(BlockSource* source) : pImpl(std::make_unique<Impl>(source)) {}
BlockHelper::BlockHelper(LevelChunk* chunk) : pImpl(std::make_unique<Impl>(chunk)) {}
BlockHelper::~BlockHelper() = default;

void BlockHelper::setBlock(BlockPos const& pos, Block const& block, uchar layer, int updateFlags) {
    return std::visit(
        [&](auto* sourceOrChunk) { block_helper::details::setBlock(sourceOrChunk, pos, block, layer, updateFlags); },
        *pImpl
    );
}
Block const& BlockHelper::getBlock(BlockPos const& pos, uchar layer) const {
    return std::visit<Block const&>(
        [&](auto const* sourceOrChunk) -> Block const& {
            return block_helper::details::getBlock(sourceOrChunk, pos, layer);
        },
        *pImpl
    );
}
DimensionHeightRange BlockHelper::getHeightRange() const {
    return std::visit(
        [](auto const* sourceOrChunk) -> DimensionHeightRange { return sourceOrChunk->mDimension.mHeightRange; },
        *pImpl
    );
}
template <>
MOD_API LevelChunk* BlockHelper::get<LevelChunk, void>() {
    if (auto res = std::get_if<LevelChunk*>(&*pImpl)) return *res;
    return nullptr;
}
template <>
MOD_API LevelChunk const* BlockHelper::get<LevelChunk, void>() const {
    if (auto res = std::get_if<LevelChunk*>(&*pImpl)) return *res;
    return nullptr;
}
template <>
MOD_API BlockSource* BlockHelper::get<BlockSource, void>() {
    if (auto res = std::get_if<BlockSource*>(&*pImpl)) return *res;
    return nullptr;
}
template <>
MOD_API BlockSource const* BlockHelper::get<BlockSource, void>() const {
    if (auto res = std::get_if<BlockSource*>(&*pImpl)) return *res;
    return nullptr;
}
} // namespace modapi::inline worldgen
