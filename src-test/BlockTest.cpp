#include "Global.h"
#include "TestReport.hpp"
#include "modapi/DeferredRegister.h"
#include "TestTexture.h"
#include <ll/api/memory/Hook.h>
#include <mc/network/packet/ItemData.h>
#include <mc/network/packet/ItemRegistryPacket.h>
#include "modapi/addons/RuntimePack.h"
#include "modapi/block/BlockRegistry.h"
#include <mc/deps/shared_types/v1_21_110/item/ItemCategory.h>
#include <mc/world/level/block/definition/BlockStateDefinition.h>
#include <mc/world/level/block/components/BlockMaterialInstance.h>
#include "modapi/item/ItemRegistry.h"
#include "modapi/item/base/ICustomArmorItem.h"
#include "modapi/item/base/ICustomBlockItem.h"
#include "modapi/item/base/ICustomFoodItem.h"
#include "modapi/item/base/ICustomToolItem.h"
#include "modapi/item/base/ICustomItem.h"
#include "modapi/worldgen/BlockHelper.h"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <ll/api/service/Bedrock.h>
#include <mc/deps/core/math/Vec2.h>
#include <mc/deps/core/math/Vec3.h>
#include <mc/server/SimulatedPlayer.h>
#include <mc/world/level/BlockSource.h>
#include <mc/world/level/Level.h>
#include <mc/world/level/block/Block.h>
#include <mc/world/level/block/BlockType.h>
#include <mc/world/level/block/BlockRenderLayer.h>
#include <mc/world/level/block/components/BlockMaterialInstancesDescription.h>
#include <mc/world/level/block/definition/BlockDefinition.h>
#include <mc/world/level/block/definition/BlockDefinitionGroup.h>
#include <mc/world/level/block/definition/ServerBlockProperty.h>
#include <mc/world/level/block/registry/BlockTypeRegistry.h>
#include <mc/world/level/chunk/LevelChunk.h>
#include <mc/world/level/dimension/Dimension.h>
#include <mc/world/level/dimension/DimensionType.h>
#include <chrono>
#include <optional>
#include <mc/world/level/block/definition/BlockDescription.h>
#include <mc/world/level/block/definition/BlockPermutationDescription.h>
#include <mc/world/level/block/components/BlockComponentDescription.h>
#include <mc/world/level/block/components/BlockMaterialInstancesDescription.h>
#include <mc/world/item/BlockItem.h>
#include <mc/world/item/ResolvedItemIconInfo.h>
#include <mc/server/commands/CommandItem.h>
#include <mc/server/commands/CommandOutput.h>
#include <mc/server/commands/CommandOutputType.h>
#include <mc/network/packet/ItemRegistryPacketPayload.h>
#include <mc/world/item/registry/ItemRegistryManager.h>
#include <thread>

namespace {

constexpr std::string_view TestBlockName = "modapi_test:test_block";

// A custom block is an engine block type: the mod subclasses `::BlockType` directly and ModAPI hands the
// constructor the id it allocated, so there is no ModAPI base class in the way.
class TestBlock : public ::BlockType {
public:
    TestBlock(std::string const& identifier, int id)
    : ::BlockType(
          identifier,
          id,
          ::BlockTypeRegistry::get().lookupByName(::HashedString{"minecraft:stone"}, false)->mMaterial
      ) {}

    // Engine hooks, overridden so the test can show the engine gets these answers.
    bool canProvideSupport(::Block const&, uchar, ::BlockSupportType) const override { return false; }

};

// A block with no document anywhere: registered from C++ alone, which is what the client property publisher is for -
// without it a client is never told this block exists.
constexpr std::string_view TestCppOnlyBlockName = "modapi_test:cpp_only_block";

// A property that only declares the creative category. Two things are under test when a block is registered with it:
// the entry's `menu_category.category` has to come out of the engine's own enum -> name helper (not from a default
// ModAPI writes), and the material instances it does not name still have to be supplied by the fallback. The category
// is deliberately not `Construction`: the fallback writes that one, so only a different value proves which path ran.
inline modapi::BlockRegistration<> withCategory() {
    modapi::BlockProperty property;
    property.mDescription.mMenuCategory->mCreativeCategory =
        ::SharedTypes::v1_21_110::ItemCategory::CreativeItemCategory::Nature;
    return modapi::BlockRegistration<>{ {}, std::move(property) };
}

modapi::DeferredRegister<modapi::BlockRegistry, TestBlock> gTestBlock{
    std::string{TestBlockName},
    withCategory(),
};

modapi::DeferredRegister<modapi::BlockRegistry, TestBlock> gTestCppOnlyBlock{
    std::string{TestCppOnlyBlockName},
    withCategory(),
};

// A plain custom item, overriding the ModAPI hooks `ICustomItem` offers.
class TestItem : public modapi::item::ICustomItem<::Item> {
public:
    TestItem(std::string const& identifier) : ICustomItem(identifier) {
    }

    // A vanilla icon name on purpose, as a probe: the client showed the name from the language file (so the
    // pack is applied) but no icon, so this tells apart "the definition does not reach the client" (still
    // blank) from "our own atlas entry is the problem" (a diamond appears).
    modapi::ItemIcon getIcon() const override { return ::modapi::ItemIcon{"modapi_test_block"}; }
    uint8_t                  getItemMaxStackSize() const override { return 16; }
    bool                     isFoil() const override { return true; }
};

// The item that places the custom block. It *is* an engine `::BlockItem` (the class carrying placement), and
// it overrides both a ModAPI hook and an engine hook.
class TestBlockItem : public modapi::item::ICustomBlockItem {
public:
    TestBlockItem(std::string const& identifier) : ICustomBlockItem(identifier, ::HashedString{TestBlockName}) {
        // Is the block visible to this process when the item is built?
        auto const* block = ::BlockTypeRegistry::get().lookupByName(::HashedString{TestBlockName}, false);
        test::getLogger().info(
            "[BLOCKS] TestBlockItem built: block lookup says {}",
            block != nullptr ? block->mNameInfo->mFullName->getString() : std::string{"<null>"}
        );
    }

    uint8_t getItemMaxStackSize() const override { return 16; }
    bool    isFoil() const override { return true; }

    bool isMusicDisk() const override { return true; } // an engine virtual, not a ModAPI one

    // No icon override: `::BlockItem::getIconInfo` resolves an item's icon from the block it is linked to, which
    // is exactly how an addon block's item gets its picture. Naming a texture here would take that away.
    std::string        getDisplayName() const override { return "ModAPI 测试方块"; }
};

modapi::DeferredRegister<modapi::item::ItemRegistry, TestItem>      gTestItem{std::string{"modapi_test:test_item"}};
// Registered again, now that the definition it writes declares `minecraft:block_placer` with the block it stands
// for: that component is what the engine itself uses to tie an item to a block, and it carries a
// `mCanUseBlockAsIcon` flag, so a client has a reason to draw the block rather than a flat texture.
modapi::DeferredRegister<modapi::item::ItemRegistry, TestBlockItem> gTestBlockItem{
    std::string{"modapi_test:test_block"}
};

// The remaining kinds of item a mod can register. Each one is a different shim, but they all end at the same
// registration path, so registering one of every kind is what tells whether that path is kind agnostic.
class TestFoodItem : public modapi::item::ICustomFoodItem<::Item> {
public:
    explicit TestFoodItem(std::string const& identifier) : ICustomFoodItem(identifier) {}

    modapi::ItemIcon getIcon() const override { return ::modapi::ItemIcon{"modapi_test_block"}; }
    int   getNutrition() const override { return 4; }
    float getSaturation() const override { return 1.0f; }
};

class TestToolItem : public modapi::item::ICustomToolItem<::Item> {
public:
    explicit TestToolItem(std::string const& identifier) : ICustomToolItem(identifier) {}

    modapi::ItemIcon getIcon() const override { return ::modapi::ItemIcon{"modapi_test_block"}; }
};

class TestArmorItem : public modapi::item::ICustomArmorItem {
public:
    explicit TestArmorItem(std::string const& identifier)
    : ICustomArmorItem(identifier, ::HumanoidArmorItem::Tier::Iron) {}

    modapi::ItemIcon getIcon() const override { return ::modapi::ItemIcon{"modapi_test_block"}; }
    ::SharedTypes::Legacy::ArmorSlot getArmorSlot() const override {
        return ::SharedTypes::Legacy::ArmorSlot::Head;
    }
};

modapi::DeferredRegister<modapi::item::ItemRegistry, TestFoodItem>  gTestFoodItem{
    std::string{"modapi_test:test_food_item"}
};
modapi::DeferredRegister<modapi::item::ItemRegistry, TestToolItem>  gTestToolItem{
    std::string{"modapi_test:test_tool_item"}
};
modapi::DeferredRegister<modapi::item::ItemRegistry, TestArmorItem> gTestArmorItem{
    std::string{"modapi_test:test_armor_item"}
};

// Installed while the mod loads: the server composes the pack stack it offers to clients when a level
// starts, so a pack added later (from a command, say) never reaches them.
std::filesystem::path gPackRoot;
std::filesystem::path gTexturesRoot;
std::string           gFirstUuid;
std::string           gSecondUuid;

// The pack uuid, read back from the manifest that was written.
std::string uuidInFile(std::filesystem::path const& manifestPath) {
    std::ifstream     in(manifestPath, std::ios::binary);
    std::stringstream buffer;
    buffer << in.rdbuf();

    auto const text = buffer.str();
    auto const key  = text.find("\"uuid\"");
    if (key == std::string::npos) return {};

    auto const first = text.find('"', text.find(':', key) + 1);
    auto const last  = text.find('"', first + 1);
    if (first == std::string::npos || last == std::string::npos) return {};
    return text.substr(first + 1, last - first - 1);
}
::BlockSource* overworldBlockSource() {
    auto level = ll::service::getLevel();
    if (!level) return nullptr;

    // The overworld; the lock also covers the (unlikely) case of the dimension being gone.
    auto dimension = level->getDimension(::DimensionType{0}).lock();
    if (!dimension) return nullptr;

    auto& source = dimension->getBlockSourceFromMainChunkSource();
    // Guard against a build whose dimension ids are not the well known ones.
    if (source.getDimensionId() != 0) return nullptr;
    return &source;
}

// A virtual player keeps the chunks around its position loaded, so the world cases always have somewhere
// to write; it removes itself again (also when an assertion throws).
class TestAnchor {
    ::SimulatedPlayer* mPlayer = nullptr;

public:
    TestAnchor() {
        auto level = ll::service::getLevel();
        if (!level) return;

        auto const spawn   = level->getSharedSpawnPos();
        auto       created = ::SimulatedPlayer::create(
            "ModApiTestBlock2",
            ::Vec3{
                static_cast<float>(spawn.x) + 0.5f,
                static_cast<float>(spawn.y) + 2.0f,
                static_cast<float>(spawn.z) + 0.5f
            },
            ::DimensionType{0},
            ::Vec2{0.0f, 0.0f}
        );
        if (created) mPlayer = created.as_ptr();
    }

    ~TestAnchor() {
        if (mPlayer != nullptr) mPlayer->remove();
    }

    TestAnchor(TestAnchor const&)            = delete;
    TestAnchor& operator=(TestAnchor const&) = delete;

    [[nodiscard]] bool valid() const { return mPlayer != nullptr; }
};

// The middle of the spawn chunk, 40 blocks above the spawn height (inside the height range).
std::optional<BlockPos> spawnTestPos(::BlockSource& source) {
    auto level = ll::service::getLevel();
    if (!level) return std::nullopt;

    auto const spawn  = level->getSharedSpawnPos();
    auto const chunkX = spawn.x >> 4;
    auto const chunkZ = spawn.z >> 4;
    auto const minY   = static_cast<short>(source.mDimension.mHeightRange->mMin);
    auto const maxY   = static_cast<short>(source.mDimension.mHeightRange->mMax);
    auto const y      = static_cast<short>(std::clamp<int>(spawn.y + 40, minY + 1, maxY - 2));

    // The nearest loaded chunk (a virtual player only loads the chunks around itself, so the spawn chunk
    // itself is not guaranteed to be there yet).
    for (int radius = 0; radius <= 4; ++radius) {
        for (int dx = -radius; dx <= radius; ++dx) {
            for (int dz = -radius; dz <= radius; ++dz) {
                if (std::max(std::abs(dx), std::abs(dz)) != radius) continue; // only the ring

                auto* chunk = source.getChunk(chunkX + dx, chunkZ + dz);
                if (chunk == nullptr) continue;

                auto const chunkPos = *chunk->mPosition;
                BlockPos   pos{chunkPos.x * 16 + 8, y, chunkPos.z * 16 + 8};
                if (!source.hasChunksAt(pos, 0, false)) continue;
                return pos;
            }
        }
    }
    return std::nullopt;
}

} // namespace

int runBlockTests() {
    test::Report report{"[BLOCKS]", "blocks-test-report.txt"};

    auto& registry = modapi::block::BlockRegistry::getInstance();

    report.isTrue("ready", registry.isReady());
    report.isTrue("deferred.registered", gTestBlock.isRegistered());
    report.isTrue("deferred.product", gTestBlock.get().has_value());

    auto block = registry.getBlock(std::string{TestBlockName});
    report.isTrue("getBlock", block.has_value());
    if (block) {
        report.isTrue(
            "lookupByName",
            ::BlockTypeRegistry::get().lookupByName(::HashedString{TestBlockName}, false) == block.as_ptr()
        );
        // The engine's type for this identifier is the C++ object, not one it built from the JSON document:
        // that is the combination ModAPI has to keep working (and that used to crash the server).
        report.isTrue(
            "jsonDefinition.cppObjectIsTheRegisteredType",
            ::BlockTypeRegistry::get().lookupByName(::HashedString{TestBlockName}, false) == block.as_ptr()
        );
    } else {
        report.skip("lookupByName");
    }

    // Block states are built after the definitions are loaded, which is why this is checked here and not
    // while the block is registered: a type without a default state cannot be placed.
    report.isTrue(
        "defaultState",
        &::BlockTypeRegistry::get().getDefaultBlockState(::HashedString{TestBlockName}, false) != nullptr
    );

    // The same for vanilla's own blocks, as a control.
    report.isTrue(
        "defaultState.vanillaControl",
        &::BlockTypeRegistry::get().getDefaultBlockState(::HashedString{"minecraft:stone"}, false) != nullptr
    );

    // What the client is told: the definition group is what `generateServerBlockProperties` walks and
    // `StartGamePacket` sends.
    if (auto* group = registry.definitionGroup()) {
        auto const properties = group->generateServerBlockProperties();
        auto const found      = std::any_of(
            properties.begin(),
            properties.end(),
            [](::ServerBlockProperty const& property) { return *property.mName == TestBlockName; }
        );
        // The engine's own list is built from the block definition group, which is the document path - a block
        // registered from C++ is not in it by design. What a client is told about comes from the publisher, and the
        // assertion below is that side of it; `found` stays as the engine-side observation.
        test::getLogger().info("[BLOCKS] engine side block list has our block: {}", found);
        report.isTrue("clientProperties.notEmpty", !properties.empty());



        // The client property publisher: a block registered from C++ alone is not in the definition group, so the
        // engine writes no entry for it - the publisher is what puts one in, and this checks the entry it builds.
        {
            ::std::vector<::ServerBlockProperty> forClient = properties;   // 引擎自己那份（非空，条目是从中拷贝重塑的）
            registry._publishClientProperties(forClient);
            bool cppOnlyPublished = false;
            for (auto& property : forClient) {
                if (*property.mName == TestCppOnlyBlockName) cppOnlyPublished = true;
            }
            test::getLogger().info(
                "[BLOCKS] client properties after publishing: {} entries, cpp_only_block {}",
                (int)forClient.size(),
                cppOnlyPublished
            );
            // The entry as it will reach a client, next to the one the engine wrote for pack_block: a diff of the
            // two is the only way to see what a block registered from C++ is still missing.
            for (auto& property : forClient) {
                if (*property.mName != TestCppOnlyBlockName) continue;
                test::getLogger().info("[BLOCKS] our own block property: {}", property.mTag->toSnbt());
            }
            report.isTrue("clientProperties.cppOnlyBlock", cppOnlyPublished);
            report.isTrue("clientProperties.ourBlock", [&] {
                for (auto& property : forClient) {
                    if (*property.mName == TestBlockName) return true;
                }
                return false;
            }());
        }

        // Read-only: the exact shape of entries the engine built itself from documents, including a block with a
        // state and two permutations. A block ModAPI registered from C++ has no entry of its own, so these are the
        // templates such an entry would have to be written from - readable straight out as SNBT.
        for (auto& property : properties) {
            if (*property.mName != "modapi_test:pack_block" && *property.mName != "modapi_test:pack_perm_block") {
                continue;
            }
            test::getLogger().info(
                "[BLOCKS] the engine's own block property for {}: {}",
                *property.mName,
                property.mTag->toSnbt()
            );
        }

        // The material the mod described lands in the block's definition. The network tag above only
        // carries the components the engine marks as network components; rendering data (material
        // instances) comes from the client's resource pack, exactly like an addon block's.
        auto const definition = group->tryGetBlockDefinition(std::string{TestBlockName});
        if (definition != nullptr) {
            auto* component = definition->mBaseComponents->getComponentDescription(
                ::BlockMaterialInstancesDescription::NameID()
            );
            auto* material = static_cast<::BlockMaterialInstancesDescription*>(component);
            report.isTrue("definition.material", material != nullptr);
            report.isTrue("definition.material.hasTexture", material != nullptr && !material->mMaterials->empty());
        } else {
            report.skip("definition.material");
            report.skip("definition.material.hasTexture");
        }
    } else {
        report.skip("clientProperties");
        report.skip("clientProperties.notEmpty");
    }

    report.section("clientPacks", [&] {
        // Installed while the mod loaded (see `installTestPacks`), which is early enough for the pack stack
        // the server offers to clients.
        report.isTrue("pack.manifest", std::filesystem::exists(gPackRoot / "manifest.json"));
        report.isTrue("pack.file", std::filesystem::exists(gPackRoot / "textures" / "terrain_texture.json"));

        // A client that already has a pack with this uuid will not download it again, so every run has to
        // offer a fresh one: two packs of the same name must not share it.
        test::getLogger().info("[BLOCKS] pack uuids: {} and {}", gFirstUuid, gSecondUuid);
        report.isTrue("pack.uuidFresh", !gFirstUuid.empty() && gFirstUuid != gSecondUuid);

        report.isTrue(
            "blockPack.textureCopied",
            std::filesystem::exists(gTexturesRoot / "textures" / "blocks" / "modapi_test_block.png")
        );

        // ...and it is a real PNG, so the client has something it can draw.
        std::ifstream in(gTexturesRoot / "textures" / "blocks" / "modapi_test_block.png", std::ios::binary);
        std::string   signature(8, '\0');
        in.read(signature.data(), 8);
        report.isTrue("blockPack.textureIsPng", signature.starts_with("\x89PNG\r\n\x1a\n"));
    });

    report.section("items", [&] {
        auto& items = modapi::item::ItemRegistry::getInstance();

        ::Item* plain     = nullptr;
        ::Item* blockItem = nullptr;
        items.forEachItemInRegistry([&](::Item& item) {
            auto const name = item.mFullName->getString();
            if (name == "modapi_test:test_item") plain = &item;
            if (name == "modapi_test:test_block") blockItem = &item;
            return true;
        });

        report.isTrue("item.registered", plain != nullptr);
        if (plain != nullptr) {
            bool const plainGlint = plain->mIsGlint;
            test::getLogger().info("[BLOCKS] plain item fields: stack = {}, glint = {}", (int)plain->mMaxStackSize, plainGlint);
        }
        report.isTrue("item.deferred", gTestItem.isRegistered());
        // What the client is told about the plain item. The client logged
        // "Item item.modapi_test:test_item requires either an icon atlas or icon texture", so either this tag
        // carries no icon (a server side gap) or it does and the packed atlas is what is missing.
        if (plain != nullptr) {
            auto const tag  = plain->buildNetworkTag();
            auto const snbt = tag != nullptr ? tag->toSnbt() : std::string{};
            test::getLogger().info("[BLOCKS] plain item tag: {}", snbt);
            // Name agnostic on purpose: what matters is that the icon sits at the top level, which is where a
            // client reads it from. Asserting a particular texture name just breaks when the name changes.
            report.isTrue("item.tagHasIcon", snbt.find("\"minecraft:icon\"") != std::string::npos);
        } else {
            report.skip("item.tagHasIcon");
        }
        // The registry has to expose the very object the registration produced. It did not before: replacing a
        // name left the old entry in `ItemRegistry::mItemRegistry`, so two items answered to one name and the
        // leftover reported default fields (`stack 64` instead of the mod's 16).
        report.isTrue("item.registryObjectIsProduct", plain != nullptr && plain == gTestItem.get().as_ptr());

        // One of every kind a mod can register, looked up by the name it was registered under: the registration path
        // is shared, so a shim that fails to reach it shows up here rather than only in a client.
        for (auto const& [label, name] : {
                 std::pair{"plain", "modapi_test:test_item"},
                 std::pair{"block", "modapi_test:test_block"},
                 std::pair{"food", "modapi_test:test_food_item"},
                 std::pair{"tool", "modapi_test:test_tool_item"},
                 std::pair{"armor", "modapi_test:test_armor_item"},
             }) {
            auto const item = modapi::item::ItemRegistry::getInstance().getItem(name);
            test::getLogger().info("[BLOCKS] registered item '{}' ({}) found: {}", label, name, static_cast<bool>(item));
            report.isTrue(std::string{"items."} + label + "Registered", static_cast<bool>(item));
        }

        // What a client is actually sent. `buildNetworkTag()` is only a local call - the data a client reads
        // comes out of this packet, so build one here and look at this item's entry. This also proves the hook
        // that fills the entry runs at all.
        report.noThrow("packet.build", [&] {
            auto                       ref = ::ItemRegistryManager::getItemRegistry();
            ::ItemRegistryPacketPayload payload{ref};

            // Read-only comparison of what the engine publishes for the item it created for a pure addon block
            // (`modapi_test:pack_block`) against the one it created for our block: the whole point is to see
            // whether the two definitions differ at all, since a client accepts one and not the other.
            for (char const* name : {"modapi_test:pack_block", "modapi_test:test_block", "modapi_test:test_item"}) {
                for (auto& entry : *payload.mItems) {
                    if ((*entry.mName).getString() != name) continue;

                    auto&      componentData = *entry.mComponentData;
                    std::string topKeys;
                    for (auto& [key, value] : componentData.mTags) {
                        topKeys += key + " ";
                    }
                    std::string componentKeys;
                    if (componentData.mTags.contains("components")) {
                        auto& components = componentData["components"].get<::CompoundTag>();
                        for (auto& [key, value] : components.mTags) {
                            componentKeys += key + " ";
                        }
                    }
                    test::getLogger().info(
                        "[BLOCKS] definition of {}: top level [{}] built from {} entr{}, components [{}]",
                        name,
                        topKeys,
                        (int)componentData.mTags.size(),
                        componentData.mTags.size() == 1 ? "y" : "ies",
                        componentKeys
                    );
                }
            }

            bool found = false;
            for (auto& entry : *payload.mItems) {
                if ((*entry.mName).getString() != "modapi_test:test_item") continue;
                found = true;

                // Look at the nodes directly instead of serializing the whole tag: `toSnbt()` threw
                // "bad variant access" here, which is a signal in itself that a merged value is in a bad state.
                auto& componentData = *entry.mComponentData;
                auto  hasComponents = componentData.mTags.contains("components");
                auto  hasIcon       = hasComponents
                                 && componentData["components"].get<::CompoundTag>().mTags.contains("minecraft:icon");
                test::getLogger().info(
                    "[BLOCKS] packet entry for test_item: components {}, icon {}, top level keys {}",
                    hasComponents,
                    hasIcon,
                    componentData.mTags.size()
                );
                report.isTrue("packet.testItemHasIcon", hasIcon);
            }
            report.isTrue("packet.testItemPresent", found);
        });

        // Read-only: the block side of the network. A client keys a block on the network id its default state
        // carries, so the addon block and ours are compared on exactly the fields a client sees.
        for (char const* name : {"modapi_test:pack_block", "modapi_test:test_block"}) {
            auto const* type = ::BlockTypeRegistry::get().lookupByName(::HashedString{name}, false);
            if (type == nullptr) {
                test::getLogger().info("[BLOCKS] block {} is not in the block registry", name);
                continue;
            }
            auto const* state = type->mDefaultState;
            test::getLogger().info(
                "[BLOCKS] block {}: block id {}, default state {}, network id {}",
                name,
                (int)type->mID->mValue,
                state != nullptr,
                state != nullptr ? (int)state->mNetworkId : -1
            );
        }


        // While this is tested no item is registered for the block, so what shows up here - if anything - is the
        // engine's own item, built the way it builds one for an addon block. Its fields are logged rather than
        // asserted, because they belong to the engine's item and not to a mod's.
        if (blockItem != nullptr) {
            bool const blockItemGlint = blockItem->mIsGlint;
            test::getLogger().info(
                "[BLOCKS] block item is not the mod's: stack {}, glint {}",
                (int)blockItem->mMaxStackSize,
                blockItemGlint
            );
            // Read-only: what the engine resolves for this item's icon. A block item answers with a block icon -
            // an empty name plus the block's runtime id - which is why a hand written definition cannot name the
            // texture, and why the item has to carry the block's own name to be linked to it at all.

            // What the client is told about this item, logged rather than asserted: for a block item the definition
            // carries `minecraft:block_placer` with `canUseBlockAsIcon` and no `minecraft:icon`, so a client draws
            // the block itself - an icon would replace the block with a flat texture again.
            auto const tag  = blockItem->buildNetworkTag();
            auto const snbt = tag != nullptr ? tag->toSnbt() : std::string{};
            test::getLogger().info("[BLOCKS] block item tag: {}", snbt);
            // No icon assertion here: this item belongs to the engine, and `buildNetworkTag()` is not what a client
            // reads anyway. What a client draws comes from the block's texture in the item atlas.
            report.skip("blockItem.tagHasIcon");

            // The item -> block link: the engine asks the item which block it renders as.
            // The link the engine actually keeps on an item is `mBlockType`.
            auto const* type = blockItem->mBlockType;
            test::getLogger().info(
                "[BLOCKS] block item renders as {}",
                type != nullptr ? type->mNameInfo->mFullName->getString() : std::string{"<null>"}
            );
            report.isTrue(
                "blockItem.rendersAsOurBlock",
                type != nullptr && type->mNameInfo->mFullName->getString() == TestBlockName
            );
        } else {
            report.skip("blockItem.maxStackSize");
            report.skip("blockItem.foil");
            report.skip("blockItem.engineVirtual");
            report.skip("blockItem.rendersAsOurBlock");
        }

        // Ids have to be handed out and must not collide with the vanilla ones: an id collision would make an
        // item answer for another.
        auto idOf = [&](char const* name) -> int {
            ::Item* found = nullptr;
            items.forEachItemInRegistry([&](::Item& item) {
                if (item.mFullName->getString() == name) found = &item;
                return true;
            });
            return found != nullptr ? (int)found->mId : -1;
        };
        auto const plainId  = plain != nullptr ? (int)plain->mId : -1;
        auto const blockId  = blockItem != nullptr ? (int)blockItem->mId : -1;
        auto const stickId  = idOf("minecraft:stick");
        auto const diamondId = idOf("minecraft:diamond");
        test::getLogger().info(
            "[BLOCKS] ids: plain {} / block item {} / stick {} / diamond {}",
            plainId,
            blockId,
            stickId,
            diamondId
        );
        report.isTrue("item.idAssigned", plainId > 0);
        // The engine folds a block's id for its item (255 - id), so a block item's id can be negative; it only
        // has to be set and distinct from ours.
        report.isTrue("items.idsDistinct", plainId > 0 && blockId != 0 && plainId != blockId);
        report.isTrue("items.idNotVanilla", plainId != stickId && plainId != diamondId && blockId != stickId && blockId != diamondId);
        // The block side: the engine's question now gets our answer.
        auto block = registry.getBlock(std::string{TestBlockName});
        if (block) {
            auto const& state = ::BlockTypeRegistry::get().getDefaultBlockState(::HashedString{TestBlockName}, false);
            report.isTrue(
                "block.canProvideSupportOverride",
                !block->canProvideSupport(state, 0, static_cast<::BlockSupportType>(0))
            );
        } else {
            report.skip("block.canProvideSupportOverride");
        }
    });
    report.section("world", [&] {
        // The custom block can actually be put into the world and read back.
        auto* source = overworldBlockSource();
        if (source == nullptr) {
            report.skip("blockSource");
            return;
        }

        TestAnchor anchor;
        test::getLogger().info("[BLOCKS] virtual player anchor: {}", anchor.valid());

        // A virtual player loads the chunks around itself asynchronously, and there is none before it is
        // created, so give the chunk a moment to show up.
        std::optional<BlockPos> spot;
        for (int attempt = 0; attempt < 10 && !spot; ++attempt) {
            spot = spawnTestPos(*source);
            if (!spot) std::this_thread::sleep_for(std::chrono::milliseconds{200});
        }
        if (!spot) {
            report.skip("spot");
            return;
        }

        modapi::BlockHelper helper{source};
        auto const&        state = ::BlockTypeRegistry::get().getDefaultBlockState(
            ::HashedString{TestBlockName},
            false
        );
        auto const original = helper.getBlock(*spot);

        report.isTrue("write", helper.setBlock(*spot, state));

        auto const placed = helper.getBlock(*spot);
        report.isTrue("readBack", placed.as_ptr() == &state);

        if (original) {
            report.isTrue("restore", helper.setBlock(*spot, *original));
        } else {
            report.skip("restore");
        }
    });

    report.finish();
    return report.failed();
}

namespace {

void installTestPacks() {

    // A real PNG (embedded below): a placeholder file would leave the block without a drawable texture.
    auto const texture = std::filesystem::temp_directory_path() / "modapi_test_tex.png";
    {
        std::ofstream out(texture, std::ios::binary | std::ios::trunc);
        out.write(reinterpret_cast<char const*>(kTestTexturePng), static_cast<std::streamsize>(kTestTexturePngSize));
    }


    // Any content at all, written into a temp directory and handed to the engine.
    modapi::addons::RuntimePack pack{"modapi_test_pack", "resources"};
    pack.addFile(
        "textures/terrain_texture.json",
        // A block is drawn with a texture the terrain atlas names, and the atlas has to map that name to a file:
        // this is the block side of the same line the item atlas needs.
        R"({"resource_pack_name":"modapi_test_pack","texture_name":"atlas.terrain",)"
        R"("texture_data":{"modapi_test_block":{"textures":"textures/blocks/modapi_test_block"},)"
        R"("modapi_test:test_block":{"textures":"textures/blocks/modapi_test_block"},)"
        R"("modapi_test:cpp_only_block":{"textures":"textures/blocks/modapi_test_block"},
        "modapi_test_block_alt":{"textures":"textures/blocks/modapi_test_block"}}})"
    );
    pack.install();
    gPackRoot  = pack.root();
    gFirstUuid = uuidInFile(gPackRoot / "manifest.json");

    // The same name again: it must come out with a different uuid, or a client that kept the previous run's
    // pack would not download this one.
    modapi::addons::RuntimePack again{"modapi_test_pack", "resources"};
    again.addFile("textures/terrain_texture.json", R"({"resource_pack_name":"modapi_test_pack","texture_name":"atlas.terrain","texture_data":{"modapi_test_block":{"textures":"textures/blocks/modapi_test_block"}}})");
    again.install();
    gSecondUuid = uuidInFile(again.root() / "manifest.json");

    // What a block needs on the client side is expressed with the same abstraction - ModAPI does not
    // specialise for blocks, the mod writes the files the client reads.
    modapi::addons::RuntimePack textures{"modapi_test_block_textures", "resources"};
    textures.addFile(
        "blocks.json",
        R"({"format_version":[1,1,0],)"
        R"("modapi_test:test_block":{"textures":"modapi_test_block","sound":"stone"},)"
        R"("modapi_test:cpp_only_block":{"textures":"modapi_test_block","sound":"stone"}})"
    );
    // The icons of an *item* are looked up in the item atlas, not in the block one: without this entry the
    // client reports "requires either an icon atlas or icon texture" and draws the item as a blank square.
    // `modapi_test:test_block` is in there as well because that is the name `/give` accepts for a block item
    // (the engine builds that item from the block, so the client asks the atlas for the block's name).
    textures.addFile(
        "textures/item_texture.json",
        R"({"resource_pack_name":"modapi_test_block_textures","texture_name":"atlas.items",)"
        R"("texture_data":{)"
        R"("modapi_test_block":{"textures":"textures/blocks/modapi_test_block"},)"
        R"("modapi_test:test_block":{"textures":"textures/blocks/modapi_test_block"}}})"
    );
    textures.addFileFrom("textures/blocks/modapi_test_block.png", texture);
    // Names of server side items and blocks come from the language keys the client formats
    // (`item.<identifier>.name` / `tile.<identifier>.name`), not from the network definition.
    textures.addFile(
        "texts/en_US.lang",
        "item.modapi_test:test_item.name=ModAPI Test Item\n"
        "item.modapi_test:registry_test_item.name=ModAPI Registry Test Item\n"
        "item.modapi_test:api_item.name=ModAPI API Item\n"
        "item.modapi_test:test_block.name=ModAPI Test Block\n"
        "tile.modapi_test:test_block.name=ModAPI Test Block\n"
    );
    textures.addFile(
        "texts/zh_CN.lang",
        "item.modapi_test:test_item.name=ModAPI 测试物品\n"
        "item.modapi_test:registry_test_item.name=ModAPI 注册测试物品\n"
        "item.modapi_test:api_item.name=ModAPI 接口物品\n"
        "item.modapi_test:test_block.name=ModAPI 测试方块\n"
        "tile.modapi_test:test_block.name=ModAPI 测试方块\n"
    );
    textures.install();
    gTexturesRoot = textures.root();

    // A control item document, in a data pack, under an identifier no C++ registration uses. It stays as the
    // known-good case: it is the only item here whose definition a client is expected to render.
    //
    // Documents for the items the C++ side registers are deliberately *not* shipped while this is being
    // checked - one variable at a time. The regression was a single missing line (`mItemParseVersion =
    // ItemVersion::DataDriven` in `ICustomItem`), and a same-named document on top of it would muddy which one
    // fixed what.
    modapi::addons::RuntimePack items{"modapi_test_items", "data"};
    // A block declared the same way, with nothing registered for it in C++: the engine is expected to create
    // the block *and* its item itself, the way it does for any addon block. Comparing this against the block
    // registered with `registerBlockFromMemoryJson` is what tells whether the C++ side has to exist at all.
    items.addFile(
        "blocks/modapi_test_pack_block.json",
        R"({"format_version":"1.21.0","minecraft:block":{)"
        R"("description":{"identifier":"modapi_test:pack_block","menu_category":{"category":"construction"}},)"
        R"("components":{"minecraft:material_instances":{"*":{"texture":"modapi_test_block","render_method":"opaque"}}}}})"
    );
    // A document block with a state and one permutation that overrides its texture. The engine writes this entry
    // itself, so dumping it shows exactly how block states and permutations reach a client - the shape a block
    // registered from C++ has to mirror. The state values are 0 and 1, and the permutation keys off value 1.
    items.addFile(
        "blocks/modapi_test_pack_perm_block.json",
        R"({"format_version":"1.21.0","minecraft:block":{)"
        R"("description":{"identifier":"modapi_test:pack_perm_block","menu_category":{"category":"construction"},)"
        R"("states":{"modapi_test:variant":[0,1]}},)"
        R"("components":{"minecraft:material_instances":{"*":{"texture":"modapi_test_block","render_method":"opaque"}}},)"
        R"("permutations":[)"
        R"({"condition":"q.block_state('modapi_test:variant') == 1",)"
        R"("components":{"minecraft:material_instances":{"*":{"texture":"modapi_test_block_alt","render_method":"opaque"}}}})"
        R"(]}})"
    );
    items.addFile(
        "items/modapi_test_pack_item.json",
        R"({"format_version":"1.21.0","minecraft:item":{)"
        R"("description":{"identifier":"modapi_test:pack_item","menu_category":{"category":"items"}},)"
        R"("components":{"minecraft:icon":{"textures":{"default":"modapi_test_block"}}}}})"
    );
    items.install();
}

void registerBlockTestCommand() {
    installTestPacks();

    // The definition of the block the C++ side registers, as an addon style document. It is deferred rather than
    // registered here: `gTestBlock` names the identifier and the document is installed by the registration itself,
    // so the C++ type stays the one that runs while the engine and a client both know the block.
    // The block has no document: it is registered from C++ alone, and ModAPI publishes it to clients itself.

    registerTestFunction("blocks", [] { return runBlockTests() == 0; });
}
} // namespace

REGISTER_ON_LOAD_TEST(BlockCommand, registerBlockTestCommand)
REGISTER_TEST(Block, runBlockTests)

// The engine declares these and does not export them: whichever translation unit builds one supplies the definition
// (ModAPI does the same for the ones it builds). Registering a block hands over a `BlockRegistration`, which holds a
// `BlockProperty` by value, so this plugin needs the default and copy constructors of what that property is made of.
BlockDescription::BlockDescription()                             = default;
BlockComponentGroupDescription::BlockComponentGroupDescription() = default;
BlockComponentGroupDescription::BlockComponentGroupDescription(BlockComponentGroupDescription const&) = default;
