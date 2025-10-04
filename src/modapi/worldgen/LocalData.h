#pragma once
#include <mc/world/level/BlockSource.h>
#include <mc/world/level/chunk/LevelChunk.h>

namespace modapi::inline worldgen {

struct LocalData {
    BlockSource*      mBlockSource = nullptr;
    LevelChunk*       mLevelChunk  = nullptr;
    Random*           mRandom      = nullptr;
    static void       init();
    static LocalData& getInstance();
};

} // namespace modapi::inline worldgen