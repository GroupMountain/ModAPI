#pragma once
#include "modapi/Macros.h"
#include "modapi/Registration.h"
#include "modapi/Registry.h"
#include "modapi/block/base/ICustomBlock.h"
#include <concepts>
#include <ll/api/event/Event.h>
#include <mc/deps/core/utility/optional_ref.h>
#include <mc/world/level/block/BlockType.h>
#include <mc/world/level/block/registry/BlockTypeRegistry.h>
#include <memory>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>

class BlockDefinition;
class BlockDefinitionGroup;
class Level;
struct ServerBlockProperty;

namespace modapi::inline block {

class BlockRegistry;

class BlockReadyEvent final : public ll::event::Event {
    BlockRegistry& mRegistry;

public:
    constexpr explicit BlockReadyEvent(BlockRegistry& registry) : mRegistry(registry) {}

    MOD_API BlockRegistry& registry() const;
};

class BlockRegistry {
public:
    struct Impl;
    std::unique_ptr<Impl> pImpl;

public:
    using EventType  = BlockReadyEvent;
    using Product    = ::BlockType;
    using ProductRef = optional_ref<::BlockType>;

public:
    BlockRegistry();
    BlockRegistry& operator=(BlockRegistry const&) = delete;
    BlockRegistry(BlockRegistry const&)            = delete;

public:
    MOD_NDAPI static BlockRegistry& getInstance();

    [[nodiscard]] MOD_NDAPI bool isReady() const noexcept;

    MOD_API static void ensureEventRegistered();

    template <class Entry>
    static constexpr bool isEntry = std::derived_from<Entry, ::BlockType> && requires { typename Entry::BlockBase; };

    template <class Entry>
    static ClassRegistrationBuilder<BlockRegistry, Entry> builder(std::string identifier) {
        return ClassRegistrationBuilder<BlockRegistry, Entry>{std::move(identifier)};
    }

    template <class Entry, class... Args>
        requires isEntry<Entry>
    ProductRef registerEntry(ClassRegistration<Entry, Args...> registration) {
        return _registerBlockWith<Entry>(registration.mIdentifier, std::move(registration.mArguments));
    }

    template <class Entry, class... Args>
        requires isEntry<Entry>
    ProductRef _registerBlockWith(std::string const& identifier, ::std::tuple<Args...> arguments) {
        auto const id = _allocateBlockId(identifier);
        if (id == 0) return {};

        try {
            auto& block = ::std::apply(
                [&](auto&&... unpacked) -> ::BlockType& {
                    return ::BlockTypeRegistry::get().registerBlock<Entry>(
                        ::HashedString{identifier},
                        id,
                        std::forward<decltype(unpacked)>(unpacked)...
                    );
                },
                std::move(arguments)
            );
            auto const& property = static_cast<Entry&>(block).getProperty();
            _applyBlockProperty(block, property);
            _storeBlockProperty(identifier, property);
            return _rememberBlock(identifier, block);
        } catch (std::exception const& e) {
            _logRegistrationFailure(identifier, e.what());
            return {};
        } catch (...) {
            _logRegistrationFailure(identifier, nullptr);
            return {};
        }
    }

    MOD_API void _applyBlockProperty(::BlockType& block, BlockProperty const& property);
    MOD_API void _storeBlockProperty(std::string const& identifier, BlockProperty property);

    MOD_API void _publishClientProperties(::std::vector<::ServerBlockProperty>& properties);

    [[nodiscard]] MOD_NDAPI ProductRef getBlock(std::string const& identifier) const;

    [[nodiscard]] MOD_NDAPI ::BlockDefinitionGroup* definitionGroup() const noexcept;

    MOD_API void _bindRegistry(::Level& level, ::BlockDefinitionGroup& group);

    MOD_NDAPI int _allocateBlockId(std::string const& identifier);

    MOD_NDAPI ProductRef _rememberBlock(std::string const& identifier, ::BlockType& block);

    MOD_API void _logRegistrationFailure(std::string const& identifier, char const* what);
};

} // namespace modapi::inline block

static_assert(modapi::Registry<modapi::BlockRegistry>);
