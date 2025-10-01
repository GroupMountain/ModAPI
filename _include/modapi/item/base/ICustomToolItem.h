#pragma once
#include <gmlib/mod/item/base/ICustomItem.h>

namespace gmlib::mod {

class ICustomToolItem : public ICustomItem {
public:
    MODAPI_NDAPI ICustomToolItem(std::string const& identifier);

    MOD_API virtual bool isSword() const;

    MOD_API virtual bool isAxe() const;

    MOD_API virtual bool isPickaxe() const;

    MOD_API virtual bool isShovel() const;

    MOD_API virtual bool isHoe() const;

    MOD_API bool isHandEquipped() const override;

    MOD_API uint8_t getItemMaxStackSize() const override;

    MOD_API bool canDestroyInCreative() const override;

    MOD_API CreativeItemCategory getCreativeCategory() const override;

    MOD_API std::string getCreativeGroup() const override;

    MOD_API Interactions::Mining::MineBlockItemEffectType getMineBlockItemEffectType() const override;

    MOD_API bool isDiggerItem() const override;

    MOD_API bool canDestroySpecial(Block const& block) const override;

    MOD_API void executeEvent(::ItemStackBase& item, ::std::string const& ev, ::RenderParams& rp) const override;

    MOD_API void _init() override;
};

} // namespace gmlib::mod
