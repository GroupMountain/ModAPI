#pragma once
#include <gmlib/Macros.h>
#include <iostream>
#include <set>
#include <vector>

namespace gmlib::mod {

struct RepairItems {
    std::set<std::string> mItems;
    float                 mRepairAmount;

    MOD_NDAPI RepairItems();
    MOD_NDAPI RepairItems(std::string_view item, float repairAmount);
    MOD_NDAPI RepairItems(std::vector<std::string> const& items, float repairAmount);
};

} // namespace gmlib::mod
