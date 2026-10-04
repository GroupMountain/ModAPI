#pragma once
#include "modapi/Macros.h"
#include "modapi/worldgen/BlockHelper.h"
#include "modapi/worldgen/base/ICustomFeature.h"
#include <ll/api/coro/Generator.h>

namespace modapi::inline worldgen {

// A feature placed by a rule rather than by a document: one instance is built per level, and each pass the level
// configures gets the engine rule that refers to it. The rule decides where its blocks go.
//
//   class MyRule : public ICustomFeatureRule {
//   public:
//       ll::coro::Generator<BlockPos> place(BlockHelper const& helper, BlockPos const& pos, Random& random) override;
//   };
//   DeferredRegister<FeatureRuleRegistry, MyRule> gRule{
//       { .mIdentifier = "mymod:my_rule", .mArguments = {}, .mPasses = {"mymod:trees"} }
//   };
class ICustomFeatureRule {
public:
    virtual ~ICustomFeatureRule()                            = default;
    ICustomFeatureRule()                                     = default;
    ICustomFeatureRule(ICustomFeatureRule const&)            = delete;
    ICustomFeatureRule& operator=(ICustomFeatureRule const&) = delete;

    // Yields the positions this rule places, in the order the generator produces them.
    MOD_NDAPI virtual ll::coro::Generator<BlockPos>
    place(BlockHelper const& helper, BlockPos const& pos, Random& random) = 0;
};

} // namespace modapi::inline worldgen
