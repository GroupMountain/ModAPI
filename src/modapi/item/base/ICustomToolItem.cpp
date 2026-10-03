#include "modapi/item/base/ICustomToolItem.h"

// The parts of the shim that build engine objects stay here rather than in the header: the engine does not export every
// constructor involved (`LevelSoundEventPacketPayload`'s, for one), so a mod's translation unit could not link them.
// The template member that needs this is a one line call.

namespace modapi::inline item {

void toolItemExecuteEvent(::ItemStackBase& item, ::std::string const& ev, ::RenderParams& rp) {
    if (rp.mActor && ev == "on_tool_used" && rp.mBlock) {
        item.hurtAndBreak(1, rp.mActor);
        // 26.51 added the `HandSlot` parameter to `Actor::swing`. `Item::executeEvent` still has no hand parameter
        // (neither does `RenderParams`), so the caller cannot know the hand - and a tool used on a block is always the
        // main hand one.
        rp.mActor->swing(ActorSwingSource::UseItem, ::HandSlot::Mainhand);
        AnimatePacket anipkt;
        anipkt.mAction    = AnimatePacket::Action::Swing;
        anipkt.mRuntimeId = rp.mActor->getRuntimeID();
        anipkt.mData      = 1.0f;
        anipkt.sendToClients();
        LevelSoundEventPacket lsepkt;
        lsepkt.mSoundEvent->mSound = ::SharedTypes::Legacy::LevelSoundEvent::ItemUseOn;
        lsepkt.mPos                = rp.mBlockPos->center();
        lsepkt.mData               = static_cast<int>(rp.mBlock->mNetworkId);
        lsepkt.mIsGlobal           = false;
        lsepkt.sendToClients();
    }
}

} // namespace modapi::inline item