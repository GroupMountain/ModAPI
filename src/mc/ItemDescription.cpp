#include "modapi/item/base/ICustomItem.h"
#include <mc/deps/core/utility/pub_sub/Subscription.h>
#include <mc/deps/shared_types/v1_21_110/item/ItemDescription.h>
#include <mc/deps/shared_types/v1_26_0/item/PlanterItemComponent.h>
#include <mc/world/item/components/PlanterItemComponent.h>
#include <mc/world/item/components/WearableItemComponent.h>

// The SDK declares these constructors but does not export them. They are defined here, inside ModAPI, so
// the templates in ICustomItem.h only name the exported factories below and never the constructors.
namespace SharedTypes::v1_21_110 {

ItemDescription::ItemDescription() = default;

} // namespace SharedTypes::v1_21_110

namespace Bedrock::PubSub {

Subscription::Subscription() = default;

} // namespace Bedrock::PubSub

namespace SharedTypes::v1_26_0 {

PlanterItemComponent::PlanterItemComponent() = default;

} // namespace SharedTypes::v1_26_0

namespace modapi::inline item {

MOD_API ::SharedTypes::v1_21_110::ItemDescription* createItemDescription() {
    return new ::SharedTypes::v1_21_110::ItemDescription();
}

MOD_API std::shared_ptr<::WearableItemComponent>
        makeWearableItemComponent(::SharedTypes::Legacy::EquipmentSlot slot, int protection) {
            auto component                  = std::make_shared<::WearableItemComponent>();
            component->mEquipmentSlot       = slot;
            component->mProtection          = protection;
            component->mHidesPlayerLocation = false;
            return component;
        }

MOD_API std::shared_ptr<::PlanterItemComponent>
        makePlanterItemComponent(std::string block, bool canUseAsIcon, bool replaceBlockItem) {
            ::SharedTypes::v1_26_0::PlanterItemComponent payload{};
            payload.mBlock->mValue        = std::move(block);
            payload.mReplaceBlockItem     = replaceBlockItem;
            payload.mAlignedPlacement     = false;
            auto component                = std::make_shared<::PlanterItemComponent>(payload);
            component->mCanUseBlockAsIcon = canUseAsIcon;
            return component;
        }

} // namespace modapi::inline item
