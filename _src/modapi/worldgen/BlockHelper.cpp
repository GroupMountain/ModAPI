#include "modapi/worldgen/BlockHelper.h"
#include "mc/world/level/BlockSource.h"
#include "mc/world/level/ChunkBlockPos.h"
#include "mc/world/level/chunk/LevelChunk.h"
#include "mc/world/level/dimension/Dimension.h"


namespace gmlib::mod {
namespace BlockHelperDetails {
bool check(const BlockSource* source, const BlockPos& pos) {
    return source->hasChunksAt(pos, 0, false) && pos.y < source->mDimension.mHeightRange->mMax
        && pos.y >= source->mDimension.mHeightRange->mMin;
}
bool check(const LevelChunk* chunk, const BlockPos& pos) {
    return pos.x - chunk->mPosition->x * 16 < 16 && pos.z - chunk->mPosition->z * 16 < 16
        && pos.y < chunk->mDimension.mHeightRange->mMax && pos.y >= chunk->mDimension.mHeightRange->mMin;
}
void setBlock(BlockSource* source, const BlockPos& pos, const Block& block, uchar layer, int updateFlags) {
    if (!check(source, pos)) throw std::runtime_error("invalid position.");
    switch (layer) {
    case 0:
        source->setBlock(pos, block, updateFlags, nullptr, nullptr);
        break;
    case 1:
        source->setExtraBlock(pos, block, updateFlags);
        break;
    default:
        throw std::out_of_range("layer out of range.");
    }
}
void setBlock(LevelChunk* chunk, const BlockPos& pos, const Block& block, uchar layer, int) {
    if (!check(chunk, pos)) throw std::runtime_error("invalid position.");
    switch (layer) {
    case 0:
        chunk->setBlock(
            ChunkBlockPos{
                static_cast<uchar>(abs(pos.x) % 16),
                static_cast<uchar>(abs(pos.z) % 16),
                pos.y - chunk->mDimension.mHeightRange->mMin
            },
            block,
            nullptr,
            nullptr
        );
        break;
    case 1:
        chunk->setExtraBlock(
            ChunkBlockPos{
                static_cast<uchar>(abs(pos.x) % 16),
                static_cast<uchar>(abs(pos.z) % 16),
                pos.y - chunk->mDimension.mHeightRange->mMin
            },
            block,
            nullptr
        );
        break;
    default:
        throw std::out_of_range("layer out of range.");
    }
}
const Block& getBlock(const BlockSource* source, const BlockPos& pos, uchar layer) {
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
const Block& getBlock(const LevelChunk* chunk, const BlockPos& pos, uchar layer) {
    if (!check(chunk, pos)) throw std::runtime_error("invalid position.");
    switch (layer) {
    case 0:
        return chunk->getBlock(
            ChunkBlockPos{
                static_cast<uchar>(abs(pos.x) % 16),
                static_cast<uchar>(abs(pos.z) % 16),
                pos.y - chunk->mDimension.mHeightRange->mMin
            }
        );
    case 1:
        return chunk->getExtraBlock(
            ChunkBlockPos{
                static_cast<uchar>(abs(pos.x) % 16),
                static_cast<uchar>(abs(pos.z) % 16),
                pos.y - chunk->mDimension.mHeightRange->mMin
            }
        );
    default:
        throw std::out_of_range("layer out of range.");
    }
}
} // namespace BlockHelperDetails
struct BlockHelper::Impl : std::variant<BlockSource*, LevelChunk*> {
    using std::variant<BlockSource*, LevelChunk*>::variant;
};
BlockHelper::BlockHelper(BlockSource* source) : pImpl(std::make_unique<Impl>(source)) {}
BlockHelper::BlockHelper(LevelChunk* chunk) : pImpl(std::make_unique<Impl>(chunk)) {}
BlockHelper::~BlockHelper() = default;

void BlockHelper::setBlock(const BlockPos& pos, const Block& block, uchar layer, int updateFlags) {
    return std::visit(
        [&](auto* sourceOrChunk) { BlockHelperDetails::setBlock(sourceOrChunk, pos, block, layer, updateFlags); },
        *pImpl
    );
}
const Block& BlockHelper::getBlock(const BlockPos& pos, uchar layer) const {
    return std::visit<const Block&>(
        [&](const auto* sourceOrChunk) -> const Block& {
            return BlockHelperDetails::getBlock(sourceOrChunk, pos, layer);
        },
        *pImpl
    );
}
DimensionHeightRange BlockHelper::getHeightRange() const {
    return std::visit(
        [](const auto* sourceOrChunk) -> DimensionHeightRange { return sourceOrChunk->mDimension.mHeightRange; },
        *pImpl
    );
}
template <>
MOD_API LevelChunk* BlockHelper::get<LevelChunk, void>() {
    if (auto res = std::get_if<LevelChunk*>(&*pImpl)) return *res;
    return nullptr;
}
template <>
MOD_API const LevelChunk* BlockHelper::get<LevelChunk, void>() const {
    if (auto res = std::get_if<LevelChunk*>(&*pImpl)) return *res;
    return nullptr;
}
template <>
MOD_API BlockSource* BlockHelper::get<BlockSource, void>() {
    if (auto res = std::get_if<BlockSource*>(&*pImpl)) return *res;
    return nullptr;
}
template <>
MOD_API const BlockSource* BlockHelper::get<BlockSource, void>() const {
    if (auto res = std::get_if<BlockSource*>(&*pImpl)) return *res;
    return nullptr;
}
} // namespace gmlib::mod
