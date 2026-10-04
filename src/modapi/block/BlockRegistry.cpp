#include "modapi/block/BlockRegistry.h"
#include "modapi/addons/RuntimePack.h"
#include "modapi/core/FileUtils.h"
#include "modapi/core/Gloabl.h"
#include "modapi/core/RegistryEvent.h"
#include <atomic>
#include <ll/api/event/EventBus.h>
#include <ll/api/memory/Hook.h>
#include <mc/deps/json/Reader.h>
#include <mc/deps/json/Value.h>
#include <mc/deps/shared_types/v1_21_110/item/ItemCategory.h>
#include <mc/deps/shared_types/v1_26_20/block/BlockEnum.h>
#include <mc/deps/shared_types/v1_26_20/block/MaterialType.h>
#include <mc/network/packet/StartGamePacket.h>
#include <mc/network/packet/StartGamePacketPayload.h>
#include <mc/util/molang/ExpressionNode.h>
#include <mc/world/level/Level.h>
#include <mc/world/level/block/BlockType.h>
#include <mc/world/level/block/components/BlockComponentDescription.h>
#include <mc/world/level/block/components/BlockMaterialInstance.h>
#include <mc/world/level/block/components/BlockMaterialInstancesDescription.h>
#include <mc/world/level/block/definition/BlockDefinition.h>
#include <mc/world/level/block/definition/BlockDefinitionGroup.h>
#include <mc/world/level/block/definition/BlockDescription.h>
#include <mc/world/level/block/definition/BlockPermutationDescription.h>
#include <mc/world/level/block/definition/BlockStateDefinition.h>
#include <mc/world/level/block/definition/ServerBlockProperty.h>
#include <system_error>

// `ServerBlockProperty` is what a client is told about a block. Its default constructor is declared in the header
// but not exported (the engine keeps its own copy internally, and the header says as much), so the definition is
// written here - which is what a declaration without an export is for.

// The engine declares default constructors for these and does not export them (the same situation as
// `ServerBlockProperty` above), so a mod that builds one for a block registered from C++ has to supply the
// definition. Every member is a `TypedStorage`, which is default constructible even where it holds a reference, so
// `= default` is the whole implementation.

namespace modapi::inline block {

// `Level::loadBlockDefinitionGroup` is where vanilla and every behaviour pack register their block
// definitions. It is virtual, so the `$` thunk is what gets hooked (the header declares that one as `MCAPI`).
//
// This is also the registration point for custom blocks, and it is early enough for everything that follows:
// the level loads its definitions before vanilla builds its items, so a block type registered right after
// the definitions is there when a block item looks for it -
// and still before the registry builds block states and before `StartGamePacket` collects the properties
// clients are told about.
LL_TYPE_INSTANCE_HOOK(
    LoadBlockDefinitionGroupHook,
    HookPriority::Normal,
    ::Level,
    &::Level::$loadBlockDefinitionGroup,
    void,
    ::Experiments const& experiments
) {
    origin(experiments);

    if (auto* group = this->getBlockDefinitions()) {
        BlockRegistry::getInstance()._bindRegistry(*this, *group);
    }
}

// A document ModAPI brought in as JSON is parsed by the engine, which stores the definition here and creates
// the block type for it afterwards. Both are left to the engine: `VanillaItems::serverInitCreativeItemsCallback`
// walks this same definition group to create each block's item and its creative entry, so a block whose type
// ModAPI replaced ends up with no working item at all.
// The packet a client gets its blocks from. `writeWithSerializationMode` is the virtual every sending path goes
// through (the same one the item definitions had to move to), and it runs before anything is serialized, so this
// is the last moment at which a missing entry can still be added.
LL_TYPE_INSTANCE_HOOK(
    ClientBlockPropertyHook,
    HookPriority::Normal,
    ::StartGamePacket,
    &::StartGamePacket::$writeWithSerializationMode,
    void,
    ::BinaryStream&                      stream,
    ::cereal::ReflectionCtx const&       reflectionCtx,
    ::std::optional<::SerializationMode> overrideMode
) {
    BlockRegistry::getInstance()._publishClientProperties(this->mBlockProperties);
    origin(stream, reflectionCtx, overrideMode);
}

struct BlockRegistry::Impl {
    ll::memory::HookRegistrar<LoadBlockDefinitionGroupHook, ClientBlockPropertyHook> mHooks;

    // The level whose definitions were loaded, and the group they live in.
    ::Level*                mLevel = nullptr;
    ::BlockDefinitionGroup* mGroup = nullptr;
    // Set when `BlockReadyEvent` is published, i.e. when block types can be registered.
    bool mReady = false;
    // The blocks this registry registered, so a mod can get them back by the name it used.
    std::unordered_map<std::string, ::BlockType*> mBlocks;
    // What each block this registry registered is telling a client, as handed over with the registration.
    std::unordered_map<std::string, BlockProperty> mBlockProperties;
};

// What a client is told about a block it has never heard of. The engine builds this same tag itself while it fills
// the packet, out of the block's definition; a block registered from C++ has no definition behind it, so the tag is
// written here instead. It mirrors, field for field, the tag the engine produces for a simple block (read out of a
// live one as SNBT): material instances under `components`, the menu category, the molang version, and the block's
// id and material.
::ServerBlockProperty _buildBlockProperty(std::string const& identifier, int blockId) {
    ::CompoundTag tag;

    auto& materialInstances       = tag["components"]["minecraft:material_instances"];
    materialInstances["mappings"] = ::CompoundTag();
    auto& instance                = materialInstances["materials"]["*"];
    instance["ambient_occlusion"] = 1.0f;
    instance["packed_bools"]      = (unsigned char)1;
    instance["render_method"]     = "opaque";
    instance["texture"]           = identifier;
    instance["tint_method"]       = "none";

    tag["menu_category"]["category"]              = "construction";
    tag["menu_category"]["category"]              = "construction";
    tag["menu_category"]["group"]                 = "";
    tag["menu_category"]["is_hidden_in_commands"] = false;

    tag["molangVersion"]                  = 12;
    tag["vanilla_block_data"]["block_id"] = blockId;
    tag["vanilla_block_data"]["material"] = "dirt";

    ::ServerBlockProperty property;
    property.mName = identifier;
    property.mTag  = std::move(tag);
    return property;
}

void BlockRegistry::_storeBlockProperty(std::string const& identifier, BlockProperty property) {
    pImpl->mBlockProperties[identifier] = std::move(property);
}

void BlockRegistry::_applyBlockProperty(::BlockType& block, BlockProperty const& property) {
    // The engine's own route for a block registered in C++: each description initialises itself against the block's
    // component storage. This is what vanilla does (`VanillaBlockTypes::registerBlocks`), and it is what makes the
    // components take effect on the server rather than only being published to a client.
    for (auto& [name, component] : *property.mComponents.mCerealDescriptions->mMap) {
        if (!component) continue;
        component->initializeComponentFromCode(block.mComponents);
    }
}

void BlockRegistry::_publishClientProperties(::std::vector<::ServerBlockProperty>& properties) {
    for (auto const& [identifier, block] : pImpl->mBlocks) {
        if (block == nullptr) continue;

        bool alreadyThere = false;
        for (auto const& written : properties) {
            if (*written.mName == identifier) {
                alreadyThere = true;
                break;
            }
        }
        if (alreadyThere) continue;

        // A property handed over with the registration decides what a client is told: the components are written by
        // the engine's own serialiser (only the networked ones, under their own names - the calls
        // `generateServerBlockProperties` makes), and the definition-level nodes come from the property's own
        // `BlockDescription` and `BlockPermutationDescription`s.
        if (auto const found = pImpl->mBlockProperties.find(identifier); found != pImpl->mBlockProperties.end()) {
            if (properties.empty()) break;            // nothing to copy the entry shape from
            properties.push_back(properties.front()); // ServerBlockProperty has no default constructor

            auto& entry = properties.back();
            entry.mName = identifier;
            *entry.mTag = ::CompoundTag();

            auto& tag = *entry.mTag;
            auto& ctx = pImpl->mLevel != nullptr ? pImpl->mLevel->cerealContext() : *pImpl->mGroup->mCtx;

            for (auto& [componentName, component] : *found->second.mComponents.mCerealDescriptions->mMap) {
                if (!component || !component->isNetworkComponent()) continue;
                tag["components"][component->getName()] = *component->buildNetworkTag(ctx);
            }
            for (auto& blockTag : *found->second.mComponents.mTags) {
                tag["blockTags"].push_back(blockTag.getString());
            }

            // A property that names no material instances gets the same default the fallback uses (the identifier as
            // the texture name), so a block is never published without a picture.
            if (!found->second.mComponents.mCerealDescriptions->mMap->contains(
                    std::string{"minecraft:material_instances"}
                )) {
                auto& instance = tag["components"]["minecraft:material_instances"]["materials"]["*"];
                tag["components"]["minecraft:material_instances"]["mappings"] = ::CompoundTag();
                instance["ambient_occlusion"]                                 = 1.0f;
                instance["packed_bools"]                                      = (unsigned char)1;
                instance["render_method"]                                     = "opaque";
                instance["texture"]                                           = identifier;
                instance["tint_method"]                                       = "none";
            }

            auto const& description = found->second.mDescription;
            auto const& vanilla     = *description.mVanillaBlockData;
            auto const& menu        = *description.mMenuCategory;

            int const   materialIndex = (int)vanilla.mMaterial;
            auto const& materialNames = ::SharedTypes::v1_26_20::BlockEnum::MaterialTypeToString();

            tag["molangVersion"]                  = found->second.mMolangVersion;
            tag["vanilla_block_data"]["block_id"] = vanilla.mBlockID != 0 ? vanilla.mBlockID : (int)block->mID->mValue;
            tag["vanilla_block_data"]["material"] = ::std::string{materialNames[materialIndex % 26]};
            tag["menu_category"]["category"] =
                ::SharedTypes::v1_21_110::ItemCategory::stringFromCreativeItemCategory(menu.mCreativeCategory);
            tag["menu_category"]["group"]                 = *menu.mCreativeGroupName;
            tag["menu_category"]["is_hidden_in_commands"] = menu.mIsHiddenInCommands;

            // Block states are published under `properties` - the node the engine writes them to, one entry per state
            // with its values. Without it a client does not know the state exists, and no permutation keying off it
            // can ever match.
            for (auto& state : *description.mStates) {
                ::CompoundTag stateTag;
                stateTag["name"] = *state.mName;
                // The storage holding a pointer member hands back the pointee itself, so this is the value list. A
                // state without values has nothing to publish, and a state is only ever declared with them.
                stateTag["enum"] = *state.mEnumValues;
                tag["properties"].push_back(stateTag);
            }

            // Permutations: a condition plus the components it overrides, written with the same per-component calls
            // the base components use.
            for (auto& permutation : found->second.mPermutations) {
                ::CompoundTag permutationTag;
                for (auto& [componentName, component] : *(*permutation.mComponents).mCerealDescriptions->mMap) {
                    if (!component || !component->isNetworkComponent()) continue;
                    permutationTag["components"][component->getName()] = *component->buildNetworkTag(ctx);
                }
                for (auto& blockTag : *(*permutation.mComponents).mTags) {
                    permutationTag["blockTags"].push_back(blockTag.getString());
                }
                permutationTag["condition"] = permutation.mCondition->getExpressionString();
                tag["permutations"].push_back(permutationTag);
            }

            continue;
        }

        properties.push_back(_buildBlockProperty(identifier, (int)block->mID->mValue));
    }
}


BlockRegistry::BlockRegistry() : pImpl(std::make_unique<Impl>()) {}

BlockRegistry& BlockRegistry::getInstance() {
    // Deliberately leaked: the hook registrar this instance owns must not unregister its hooks
    // while the process/DLL is being torn down.
    static BlockRegistry& instance = *new BlockRegistry();
    return instance;
}

bool BlockRegistry::isReady() const noexcept { return pImpl->mReady; }

::Level* BlockRegistry::level() const noexcept { return pImpl->mLevel; }

::BlockDefinitionGroup* BlockRegistry::definitionGroup() const noexcept { return pImpl->mGroup; }

void BlockRegistry::ensureEventRegistered() {
    // The vanilla hooks live in the registry's implementation, so the singleton has to exist before the pass
    // that fires them; every consumer goes through this entry point.
    (void)getInstance();
    static std::atomic_bool registered = false;
    core::ensureRegistryEventRegistered<EventType>(registered);
}

void BlockRegistry::_bindRegistry(::Level& level, ::BlockDefinitionGroup& group) {
    pImpl->mLevel = &level;
    pImpl->mGroup = &group;

    core::getLogger().info(
        "BlockRegistry: this level loaded {} block definitions.",
        group.getBlockDefinitions().size()
    );

    // A definition of ours that a pack also brought in (or one we never owned) would leave the engine with a
    // type of its own next to ours; say so loudly rather than letting the two disagree.
    for (auto const& [identifier, block] : pImpl->mBlocks) {
        if (group.mBlockDefinitions->contains(identifier)) {
            core::getLogger().error(
                "BlockRegistry: '{}' is also defined by a behaviour pack that ModAPI does not own; the engine "
                "and this C++ block are likely to disagree about it.",
                identifier
            );
        }
    }

    if (pImpl->mReady) return;

    pImpl->mReady = true;
    core::getLogger().info("BlockRegistry: block types can be registered now.");
    ll::event::EventBus::getInstance().publish(BlockReadyEvent{*this});
}

int BlockRegistry::_allocateBlockId(std::string const& identifier) {
    if (!pImpl->mReady || pImpl->mGroup == nullptr) {
        core::getLogger().error(
            "BlockRegistry: blocks can only be registered once a level loaded its block definitions; "
            "use DeferredRegister<BlockRegistry, ...> to register blocks before the server starts."
        );
        return 0;
    }

    // The engine keeps the first type registered under a name and silently drops the second, which leaves a
    // mod with an object that is not the registered block; refuse instead of producing that state.
    if (::BlockTypeRegistry::get().lookupByName(::HashedString{identifier}, false) != nullptr) {
        core::getLogger().error("BlockRegistry: '{}' is already registered as a block type.", identifier);
        return 0;
    }
    if (pImpl->mGroup->mBlockDefinitions->contains(identifier)) {
        core::getLogger().error(
            "BlockRegistry: '{}' is already defined by a behaviour pack that ModAPI does not own; a block "
            "registered in C++ cannot share its identifier with one.",
            identifier
        );
        return 0;
    }

    // The definition group's counter starts above the vanilla block ids - the range the engine uses for blocks
    // that did not come with the base game.
    auto const id = ++pImpl->mGroup->mLastBlockId;
    return id;
}

BlockRegistry::ProductRef BlockRegistry::_rememberBlock(std::string const& identifier, ::BlockType& block) {
    pImpl->mBlocks[identifier] = &block;
    return &block;
}

BlockRegistry::ProductRef BlockRegistry::getBlock(std::string const& identifier) const {
    auto it = pImpl->mBlocks.find(identifier);
    return it != pImpl->mBlocks.end() ? it->second : nullptr;
}

void BlockRegistry::_logRegistrationFailure(std::string const& identifier, char const* what) {
    if (what != nullptr) {
        core::getLogger().error("BlockRegistry: could not register the block '{}': {}", identifier, what);
    } else {
        core::getLogger().error("BlockRegistry: could not register the block '{}'.", identifier);
    }
}

BlockRegistry& BlockReadyEvent::registry() const { return mRegistry; }

} // namespace modapi::inline block

// The engine declares default constructors and copy operations for these block definition types and does not export
// them, so whichever translation unit builds one supplies the definition it needs. These are the ones ModAPI builds.
// Default constructors only, and copy/assignment only where the linker asks for it: a type holding a `CompoundTag`
// (`ServerBlockProperty`) keeps the engine's exported copy, because a memberwise `= default` copy of that one would
// share the tag between two objects.
BlockDescription::BlockDescription()                                                                  = default;
BlockComponentGroupDescription::BlockComponentGroupDescription()                                      = default;
BlockComponentGroupDescription::BlockComponentGroupDescription(BlockComponentGroupDescription const&) = default;
BlockPermutationDescription::BlockPermutationDescription()                                            = default;
ExpressionNode::ExpressionNode()                                                                      = default;
ServerBlockProperty::ServerBlockProperty()                                                            = default;
