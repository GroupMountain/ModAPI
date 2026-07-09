#include <mc/world/item/ItemInstance.h>
#include <mc/world/item/NetworkItemInstanceDescriptor.h>
#include <mc/world/item/crafting/Recipe.h>

// Definitions for symbols not exported in LeviLamina 26.20.0 server

Recipe::Results::Results() : mResultsAreLoaded(false) {}

Recipe::Recipe()
: mRecipeId(),
  mMyId(),
  mWidth(0),
  mHeight(0),
  mPriority(0),
  mRecipeNetId(0u),
  mMyIngredients(),
  mResults(std::vector<::ItemInstance>{}),
  mUnlockingRequirement(),
  mRecipeDataVersion(),
  mTag() {}
