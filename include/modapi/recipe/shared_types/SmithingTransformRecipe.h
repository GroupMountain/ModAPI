#pragma once
#include "modapi/recipe/shared_types/ShapelessRecipe.h"

// Mirrors mc/world/item/crafting/SmithingTransformRecipe.h of LeviLamina 26.40.0.
// The 26.40.0 header guards the constructor with LL_PLAT_C (client only) because the server
// binary no longer exports the symbol, so a default constructor is defined here instead; the
// members of the recipe are filled in by makeCustomSmithingRecipe.
class SmithingTransformRecipe : public ::ShapelessRecipe {
public:
    ::Recipe::ResultList mRuntimeResults;

public:
    SmithingTransformRecipe() : ShapelessRecipe() {}

public:
    bool matches(::CraftingContainer const& craftingContainer, ::CraftingContext const&) const override;

    ::std::vector<::ItemInstance> const&
    assemble(::CraftingContainer& craftingContainer, ::CraftingContext&) const override;

    ~SmithingTransformRecipe() override = default;

public:
    MCAPI static ::mce::UUID const& ID();
};
