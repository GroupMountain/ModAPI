#include "modapi/worldgen/ICustomFeature.h"
#include "modapi/worldgen/LocalData.h"

namespace modapi::inline worldgen {

ICustomFeature::ICustomFeature()  = default;
ICustomFeature::~ICustomFeature() = default;
std::optional<BlockPos> ICustomFeature::place(IFeature::PlacementContext const& context) const {
    std::shared_ptr<BlockHelper> helper{};
    if (LocalData::getInstance().mBlockSource) {
        helper = std::make_shared<BlockHelper>(LocalData::getInstance().mBlockSource);
    } else {
        helper = std::make_shared<BlockHelper>(LocalData::getInstance().mLevelChunk);
    }
    return this->place(*helper, context.mUnke9f615.as<BlockPos>(), *LocalData::getInstance().mRandom);
}

} // namespace modapi::inline worldgen