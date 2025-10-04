#pragma once
#include <ll/api/io/PatternFormatter.h>
#include <mc/deps/core/math/Color.h>

namespace modapi::inline core {

class RandomColorLogFormatter : public ll::io::PatternFormatter {
public:
    using PatternFormatter::PatternFormatter;

    void format(ll::io::LogMessageView const& view, std::string& buffer) const noexcept override;

    mce::Color hslToRgb(double h, double s, double l) const noexcept;
    mce::Color generateRandomBrightColor() const noexcept;
};

} // namespace modapi::inline core