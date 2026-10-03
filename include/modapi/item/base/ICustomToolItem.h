#pragma once
#include "modapi/item/base/ICustomItem.h"
#include <mc/deps/core/math/Vec3.h>
#include <mc/deps/core/string/HashedString.h>
#include <mc/legacy/ActorRuntimeID.h>
#include <mc/network/packet/AnimatePacket.h>
#include <mc/network/packet/LevelSoundEventPacket.h>
#include <mc/world/actor/Actor.h>
#include <mc/world/actor/ActorSwingSource.h>
#include <mc/world/actor/RenderParams.h>
#include <mc/world/item/HandSlot.h>
#include <mc/world/item/ItemStackBase.h>
#include <mc/world/item/ItemTag.h>
#include <mc/world/level/BlockPos.h>
#include <mc/world/level/block/Block.h>

namespace modapi::inline item {

// Defined in the `.cpp`: it builds engine packets, some of whose constructors the engine does not export.
MOD_API void toolItemExecuteEvent(::ItemStackBase& item, ::std::string const& ev, ::RenderParams& rp);

// A tool item, on top of whatever engine base `T` is. Every answer is written here rather than in a `.cpp`: a template
// member's definition has to be visible wherever `T` is instantiated, and its mangled name moves with `T`.
template <typename T>
class ICustomToolItem : public ICustomItem<T> {
public:
    explicit ICustomToolItem(std::string const& identifier) : ICustomItem<T>(identifier) {
        this->addTag(ItemTag{ItemTag{"minecraft:is_tool"}});
    }

    virtual bool isSword() const { return false; }

    virtual bool isAxe() const { return false; }

    virtual bool isPickaxe() const { return false; }

    virtual bool isShovel() const { return false; }

    virtual bool isHoe() const { return false; }

    bool isHandEquipped() const override { return true; }

    uint8_t getItemMaxStackSize() const override { return 1; }

    bool canDestroyInCreative() const override { return !isSword(); }

    ::SharedTypes::CreativeItemCategory getCreativeCategory() const override {
        return ::SharedTypes::CreativeItemCategory::Equipment;
    }

    std::string getCreativeGroup() const override {
        if (isSword()) return "itemGroup.name.sworde";
        if (isAxe()) return "itemGroup.name.axe";
        if (isPickaxe()) return "itemGroup.name.pickaxe";
        if (isShovel()) return "itemGroup.name.shovel";
        if (isHoe()) return "itemGroup.name.hoe";
        return ICustomItem<T>::getCreativeGroup();
    }

    Interactions::Mining::MineBlockItemEffectType getMineBlockItemEffectType() const override {
        return Interactions::Mining::MineBlockItemEffectType::DiggerItem;
    }

    bool isDiggerItem() const override { return true; }

    bool canDestroySpecial(Block const& block) const override {
        bool result = T::canDestroySpecial(block);
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

    void executeEvent(::ItemStackBase& item, ::std::string const& ev, ::RenderParams& rp) const override {
        // One line, because the body builds engine packets whose constructors the engine does not export: it lives in
        // the `.cpp`, where the whole thing is compiled once inside ModAPI.
        toolItemExecuteEvent(item, ev, rp);
    }
    void _init() {
        ICustomItem<T>::_init();
        if (isSword()) this->addTag(ItemTag{"minecraft:is_sword"});
        if (isAxe()) this->addTag(ItemTag{"minecraft:is_axe"});
        if (isPickaxe()) this->addTag(ItemTag{"minecraft:is_pickaxe"});
        if (isShovel()) this->addTag(ItemTag{"minecraft:is_shovel"});
        if (isHoe()) this->addTag(ItemTag{"minecraft:is_hoe"});
    }
};

} // namespace modapi::inline item