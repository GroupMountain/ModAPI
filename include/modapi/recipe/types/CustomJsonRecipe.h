#pragma once
#include "modapi/recipe/base/ICustomRecipe.h"

namespace modapi::inline recipe {

// A recipe document that is loaded by the engine's own JSON loader. The registry specialises this entry so
// the document is handed to `registerRecipeFromMemoryJson` rather than being turned into a `::Recipe` here.
class CustomJsonRecipe : public ICustomRecipe {
    std::string mRecipeId;
    std::string mRawJson;

public:
    CustomJsonRecipe(std::string recipeId, std::string rawJson)
    : mRecipeId(std::move(recipeId)),
      mRawJson(std::move(rawJson)) {}

    std::string getRecipeId() const override { return mRecipeId; }

    std::vector<std::string> getCraftingTags() const override { return {}; }

    std::string const& json() const { return mRawJson; }

protected:
    void _init() override {}
};

} // namespace modapi::inline recipe
