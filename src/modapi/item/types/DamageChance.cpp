#include "modapi/item/types/DamageChance.h"
#include <ll/api/utils/RandomUtils.h>

namespace modapi::inline item {

DamageChance::DamageChance() = default;

DamageChance::DamageChance(uint8_t min, uint8_t max) : mMin(min), mMax(max) {}

int DamageChance::random() const {
    auto chance = ll::random_utils::rand<uint8_t>(mMin, mMax);
    if (chance > 100) chance = 100;
    return chance;
}

} // namespace modapi::inline item
