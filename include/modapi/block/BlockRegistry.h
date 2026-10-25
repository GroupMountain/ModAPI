#pragma once
#include "modapi/Macros.h"
#include "modapi/Registry.h"
#include "modapi/core/FileUtils.h"
#include <concepts>
#include <filesystem>
#include <ll/api/event/Event.h>
#include <mc/deps/core/utility/optional_ref.h>
#include <mc/deps/nbt/CompoundTag.h>
#include <mc/world/level/block/BlockType.h>
#include <mc/world/level/block/components/BlockComponentDescription.h>
#include <mc/world/level/block/definition/BlockComponentGroupDescription.h>
#include <mc/world/level/block/definition/BlockDescription.h>
#include <mc/world/level/block/definition/BlockPermutationDescription.h>
#include <mc/world/level/block/registry/BlockTypeRegistry.h>
#include <memory>
#include <string>
#include <utility>
#include <vector>

class BlockDefinition;
class BlockDefinitionGroup;
class BlockType;
class Level;

struct ServerBlockProperty;
class BlockDefinitionGroup;

namespace modapi::inline block {

class BlockRegistry;

// What a client is told about a block, in the engine's own types.
struct BlockProperty {
    // Each description initialises itself into the block's component storage (`initializeComponentFromCode`,
    // which is what makes it take effect on the server) and is serialised for a client under `components`
    // (`mTags` becomes `blockTags`).
    ::BlockComponentGroupDescription mComponents;
    // Publishes `menu_category`, `vanilla_block_data` and `traits`.
    ::BlockDescription mDescription;
    // Publishes `permutations`.
    ::std::vector<::BlockPermutationDescription> mPermutations;
    // Published as `molangVersion`: a scalar on the definition, with no structure of its own.
    int mMolangVersion = 12;
    // Merged over the entry last, for anything the fields above do not cover.
    ::CompoundTag mExtra;
};

// What a registration carries: how to build the block, and what to tell a client about it.
template <class... Args>
struct BlockRegistration {
    ::std::tuple<Args...> mArguments;
    BlockProperty         mProperty;
};


// Published once a level has loaded its block definitions, i.e. after vanilla and every behaviour pack
// registered theirs. That is the moment a custom block type is registered, and it is early enough that:
//
//   - a block item still finds its block while the engine builds items,
//   - `BlockTypeRegistry::prepareBlocks` still builds the block's states,
//   - `StartGamePacket` collects the properties a client is told about after this point, so the block is in them.
class BlockReadyEvent final : public ll::event::Event {
    BlockRegistry& mRegistry;

public:
    constexpr explicit BlockReadyEvent(BlockRegistry& registry) : mRegistry(registry) {}

    MOD_API BlockRegistry& registry() const;
};

// Custom blocks: the behaviour is C++, and so is what a client is told about them.
//
// The behaviour: the mod derives from `::BlockType` itself and ModAPI registers that type
// with the engine, exactly like vanilla gives every block its own subclass:
//
//   class MyBlock : public ::BlockType {
//   public:
//       MyBlock(std::string const& identifier, int id, ::Material const& material)
//       : ::BlockType(identifier, id, material) {}
//   };
//   static modapi::DeferredRegister<modapi::BlockRegistry, MyBlock> gMyBlock{
//       std::string{"mymod:my_block"},
//       modapi::BlockRegistration<>{{}, modapi::BlockProperty{/* ... */}}
//   };
//
// `BlockTypeRegistry::registerBlock<MyBlock>` constructs the type with `(identifier, id, ...)` and takes
// ownership of it, so a mod's constructor receives the id ModAPI allocated; ids come from the level's
// `BlockDefinitionGroup::mLastBlockId`, whose counter starts above the vanilla ids (10001 in the build this
// was written against).
//
// The definition a client parses - `minecraft:material_instances`, the menu category, states, permutations -
// is described by the `BlockProperty` the registration carries, and no document is involved: the property is
// made of the engine's own types, `_applyBlockProperty` gives it to the running block and
// `_publishClientProperties` gives it to a client. Textures, models and sound still come from the mod's own
// resource pack.
//
// A name that already belongs to a block - registered as a type, or defined by a behaviour pack ModAPI does not
// own - is refused rather than registered on top of: the engine keeps the first one, which would leave the mod
// holding an object that is not its block.
// True for the registration a mod hands over: how to build the block plus what to tell a client about it.
template <class T>
struct IsBlockRegistration : ::std::false_type {};
template <class... Args>
struct IsBlockRegistration<BlockRegistration<Args...>> : ::std::true_type {};

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

    // True once a level's block definitions are loaded, i.e. once custom blocks can be added.
    [[nodiscard]] MOD_NDAPI bool isReady() const noexcept;

    // Registers the ready event emitter (idempotent); called by `ModAPI::load()` and by every
    // `DeferredRegister<BlockRegistry, ...>`.
    MOD_API static void ensureEventRegistered();

    // Entry types are `::BlockType` implementations.
    template <class Entry>
    static constexpr bool isEntry = std::derived_from<Entry, ::BlockType>;

    // ---------------------------------------------------------------------------------------------
    // A block is a `::BlockType` subclass registered under an identifier, and what a client is told about it is a
    // `BlockProperty`, handed over with the registration (never set separately):
    //
    //   modapi::BlockRegistry::registerBlock<MyBlock>("mymod:my_block", {
    //       .mArguments = { /* extra arguments for MyBlock's constructor */ },
    //       .mProperty  = { .mComponents = ..., .mDescription = ..., .mPermutations = ... },
    //   });
    //
    // `BlockProperty` is made of the engine's own types, and the same property drives both sides: its components are
    // injected into the running block type (`initializeComponentFromCode`, the way vanilla registers its own blocks)
    // and serialised for a client - only the networked components, under their own names (`isNetworkComponent`,
    // `getName`, `buildNetworkTag`), which is what `_publishClientProperties` writes. A block registered without a
    // property still gets an entry, built from defaults.
    // ---------------------------------------------------------------------------------------------
    // Builds `Entry` under `identifier`, applies `registration.mProperty` to it, and remembers both. `identifier`
    // and the block id are ModAPI's to supply, so `mArguments` are only the extra constructor arguments.
    template <std::derived_from<::BlockType> Entry, class... Args>
    static ProductRef registerBlock(std::string const& identifier, BlockRegistration<Args...> registration) {
        auto property = registration.mProperty;
        return getInstance()
            .template _registerBlockWith<Entry>(identifier, std::move(property), std::move(registration.mArguments));
    }

    // Builds `Entry` under an identifier ModAPI is given directly; `DeferredRegister` goes through here. The single
    // argument may be a `BlockRegistration`, whose arguments go to the constructor and whose property is applied;
    // without one the block is registered with a default property.
    template <std::derived_from<::BlockType> Entry, class... Args>
    ProductRef registerEntry(std::string const& identifier, Args&&... args) {
        if constexpr (sizeof...(Args) == 1 && (IsBlockRegistration<::std::remove_cvref_t<Args>>::value && ...)) {
            auto registration = std::get<0>(::std::forward_as_tuple(std::forward<Args>(args)...));
            auto arguments    = std::move(registration.mArguments);
            return getInstance().template _registerBlockWith<Entry>(
                identifier,
                std::move(registration.mProperty),
                std::move(arguments)
            );
        } else {
            return getInstance().template _registerBlockWith<Entry>(
                identifier,
                BlockProperty{},
                ::std::forward_as_tuple(std::forward<Args>(args)...)
            );
        }
    }

    // Registers a block and applies the property: the type is built, the components are injected into it (which is
    // what makes them take effect on the server), and both the block and its property are remembered.
    template <std::derived_from<::BlockType> Entry, class... Args>
    ProductRef
    _registerBlockWith(std::string const& identifier, BlockProperty property, ::std::tuple<Args...> arguments) {
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
            _applyBlockProperty(block, property);
            _storeBlockProperty(identifier, std::move(property));
            return _rememberBlock(identifier, block);
        } catch (std::exception const& e) {
            _logRegistrationFailure(identifier, e.what());
            return {};
        } catch (...) {
            _logRegistrationFailure(identifier, nullptr);
            return {};
        }
    }

    // Injects the property's components into a running block (the engine's own route: `initializeComponentFromCode`
    // with the block's component storage), and keeps the property for the entry a client is sent.
    MOD_API void _applyBlockProperty(::BlockType& block, BlockProperty const& property);
    MOD_API void _storeBlockProperty(std::string const& identifier, BlockProperty property);

    // Adds an entry to the properties a client is told about for every block ModAPI registered itself, when the
    // engine did not produce one for it. The engine builds that list in `StartGamePacketPayload` out of the block
    // definition group, so a block whose type ModAPI created gets no entry of its own - and a client that is not
    // told about a block does not know it. Called from the packet's write path, before anything of the packet has
    // been serialized.
    MOD_API void _publishClientProperties(::std::vector<::ServerBlockProperty>& properties);

    // The block type registered under `identifier`, or nothing.
    [[nodiscard]] MOD_NDAPI ProductRef getBlock(std::string const& identifier) const;

    // The level whose definitions were loaded, and the group they live in.
    [[nodiscard]] MOD_NDAPI ::Level*                level() const noexcept;
    [[nodiscard]] MOD_NDAPI ::BlockDefinitionGroup* definitionGroup() const noexcept;

    // Remembers the level and its definition group, and publishes `BlockReadyEvent` once.
    MOD_API void _bindRegistry(::Level& level, ::BlockDefinitionGroup& group);

    // Next id the engine would hand out; 0 when the registry is not ready or the name is already taken.
    MOD_NDAPI int _allocateBlockId(std::string const& identifier);

    // Remembers a block this registry registered.
    MOD_NDAPI ProductRef _rememberBlock(std::string const& identifier, ::BlockType& block);

    MOD_API void _logRegistrationFailure(std::string const& identifier, char const* what);
};

} // namespace modapi::inline block

static_assert(modapi::Registry<modapi::BlockRegistry>);
