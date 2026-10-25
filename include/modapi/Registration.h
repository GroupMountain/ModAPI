#pragma once
#include "modapi/Registry.h"
#include <concepts>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>

namespace modapi {

// A class entry is built by the registry from an identifier and the stored constructor arguments.
template <class Entry, class... Args>
struct ClassRegistration {
    std::string         mIdentifier;
    std::tuple<Args...> mArguments;
};

// A data entry is a value the registry already knows how to consume; no user subclass is involved.
template <class Entry>
struct DataRegistration {
    Entry mEntry;
};

template <Registry T, class Entry, class... Args>
class ClassRegistrationBuilder {
    std::string         mIdentifier;
    std::tuple<Args...> mArguments;

public:
    explicit ClassRegistrationBuilder(std::string identifier) : mIdentifier(std::move(identifier)), mArguments() {}

    ClassRegistrationBuilder(std::string identifier, std::tuple<Args...> arguments)
    : mIdentifier(std::move(identifier)),
      mArguments(std::move(arguments)) {}

    template <class... A>
    [[nodiscard]] auto arguments(A&&... args) const {
        return ClassRegistrationBuilder<T, Entry, Args..., std::decay_t<A>...>{
            mIdentifier,
            std::tuple_cat(mArguments, std::make_tuple(std::forward<A>(args)...))
        };
    }

    [[nodiscard]] ClassRegistration<Entry, Args...> build() const { return {mIdentifier, mArguments}; }
};

template <Registry T>
class DataRegistrationBuilder {
    std::string          mIdentifier;
    typename T::Property mProperty{};

public:
    explicit DataRegistrationBuilder(std::string identifier) : mIdentifier(std::move(identifier)) {}

    DataRegistrationBuilder(std::string identifier, typename T::Property property)
    : mIdentifier(std::move(identifier)),
      mProperty(std::move(property)) {}

    DataRegistrationBuilder& property(typename T::Property value) {
        mProperty = std::move(value);
        return *this;
    }

    [[nodiscard]] DataRegistration<typename T::DataEntry> build() const {
        return {T::makeDataEntry(mIdentifier, mProperty)};
    }
};

} // namespace modapi
