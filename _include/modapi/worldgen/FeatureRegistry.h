#pragma once
#include <modapi/worldgen/ICustomFeature.h>
#include <ll/api/coro/Generator.h>
#include <memory>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>


namespace gmlib::mod {
MOD_API void registerFeature(const std::string& identifier, std::unique_ptr<IFeature> feature);
MOD_API void registerFeatureRule(
    const std::string&                                                                         identifier,
    const std::vector<std::string>&                                                            passes,
    const std::string&                                                                         placeFeature,
    std::function<ll::coro::Generator<BlockPos>(const BlockHelper&, const BlockPos&, Random&)> rule
);
// register data driven feature and feature rule
MOD_API void registerFeature(const nlohmann::json& json);
MOD_API void registerFeatureRule(const nlohmann::json& json);
} // namespace gmlib::mod