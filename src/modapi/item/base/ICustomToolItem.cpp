#include "modapi/item/base/ICustomToolItem.h"
#include <mc/deps/core/math/Vec3.h>
#include <mc/deps/core/string/HashedString.h>
#include <mc/legacy/ActorRuntimeID.h>
#include <mc/network/packet/AnimatePacket.h>
#include <mc/network/packet/LevelSoundEventPacket.h>
#include <mc/world/actor/Actor.h>
#include <mc/world/actor/ActorSwingSource.h>
#include <mc/world/actor/RenderParams.h>
#include <mc/world/item/ItemStackBase.h>
#include <mc/world/item/ItemTag.h>
#include <mc/world/level/BlockPos.h>
#include <mc/world/level/block/Block.h>

namespace modapi::inline item {

ICustomToolItem::ICustomToolItem(std::string const& identifier) : ICustomItem(identifier) {
    addTag(ItemTag{ItemTag{"minecraft:is_tool"}});
}

bool ICustomToolItem::isSword() const { return false; }

bool ICustomToolItem::isAxe() const { return false; }

bool ICustomToolItem::isPickaxe() const { return false; }

bool ICustomToolItem::isShovel() const { return false; }

bool ICustomToolItem::isHoe() const { return false; }

bool ICustomToolItem::isHandEquipped() const { return true; }

uint8_t ICustomToolItem::getItemMaxStackSize() const { return 1; }

bool ICustomToolItem::canDestroyInCreative() const { return !isSword(); }

CreativeItemCategory ICustomToolItem::getCreativeCategory() const { return CreativeItemCategory::Equipment; }

std::string ICustomToolItem::getCreativeGroup() const {
    if (isSword()) return "itemGroup.name.sworde";
    if (isAxe()) return "itemGroup.name.axe";
    if (isPickaxe()) return "itemGroup.name.pickaxe";
    if (isShovel()) return "itemGroup.name.shovel";
    if (isHoe()) return "itemGroup.name.hoe";
    return ICustomItem::getCreativeGroup();
}

Interactions::Mining::MineBlockItemEffectType ICustomToolItem::getMineBlockItemEffectType() const {
    return Interactions::Mining::MineBlockItemEffectType::DiggerItem;
}

bool ICustomToolItem::isDiggerItem() const { return true; }

bool ICustomToolItem::canDestroySpecial(Block const& block) const {
    bool result = ICustomItem::canDestroySpecial(block);
    if (!result) {
        if (isSword()) {
            result = result || block.hasTag(HashedString("minecraft:is_sword_item_destructible"));
        }
        if (isAxe()) {
            result = result || block.hasTag(HashedString("minecraft:is_axe_item_destructible"));
        }
        if (isPickaxe()) {
            result = result || block.hasTag(HashedString("minecraft:is_pickaxe_item_destructible"));
        }
        if (isShovel()) {
            result = result || block.hasTag(HashedString("minecraft:is_shovel_item_destructible"));
        }
        if (isHoe()) {
            result = result || block.hasTag(HashedString("minecraft:is_hoe_item_destructible"));
        }
    }
    return result;
}

void ICustomToolItem::executeEvent(::ItemStackBase& item, ::std::string const& ev, ::RenderParams& rp) const {
    if (rp.mActor && ev == "on_tool_used" && rp.mBlock) {
        item.hurtAndBreak(1, rp.mActor);
        rp.mActor->swing(ActorSwingSource::UseItem);
        AnimatePacket anipkt;
        anipkt.mAction    = AnimatePacket::Action::Swing;
        anipkt.mRuntimeId = rp.mActor->getRuntimeID();
        anipkt.mData      = 1.0f;
        anipkt.sendToClients();
        LevelSoundEventPacket lsepkt;
        lsepkt.mEventId  = ::SharedTypes::Legacy::LevelSoundEvent::ItemUseOn;
        lsepkt.mPos      = rp.mBlockPos->center();
        lsepkt.mData     = static_cast<int>(rp.mBlock->mNetworkId);
        lsepkt.mIsGlobal = false;
        lsepkt.sendToClients();
    }
}

void ICustomToolItem::_init() {
    ICustomItem::_init();
    if (isSword()) addTag(ItemTag{"minecraft:is_sword"});
    if (isAxe()) addTag(ItemTag{"minecraft:is_axe"});
    if (isPickaxe()) addTag(ItemTag{"minecraft:is_pickaxe"});
    if (isShovel()) addTag(ItemTag{"minecraft:is_shovel"});
    if (isHoe()) addTag(ItemTag{"minecraft:is_hoe"});
}

} // namespace modapi::inline item
