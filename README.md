# ModAPI

ModAPI 是 GroupMountain 维护的 Minecraft Bedrock Dedicated Server 模组开发接口库，面向 LeviLamina 服务器扩展。它用 C++20 提供一组注册表，统一以 `builder<T>("id")...build()` 描述条目。

## 功能概览

- 物品：`ICustomItem<T>` 模板，组件与物品描述直接挂在物品上
- 方块：`ICustomBlock<T>` 模板，组件、状态、排列、Molang 版本直接挂在方块上
- 配方：普通配方是可直接构造的 `Custom*Recipe`；用户行为配方继承 `ICustom*Recipe`
- 数据注册表：战利品表、游戏规则、生物群系、创造栏分组
- 世界生成：特征、特征规则、地表构建器
- 运行时资源包/行为包：`modapi::addons::RuntimePack`

## 环境要求

- Windows x64
- [xmake](https://xmake.io/)
- clang-cl（LLVM 23）与 Python
- C++20

## 构建

```bash
xmake f -m release -a x64 -p windows
xmake build ModAPI
```

`after_load` 会运行 `scripts/header_file_patch.py`，把 SDK 的 `HumanoidArmorItem` 修补到 `include/mc/world/item/HumanoidArmorItem.h`。产物：

- `bin/dll/ModAPI/ModAPI.dll`
- `bin/test/dll/test/test.dll`（执行 `xmake build test` 后）
- `bin/include/`

## 延迟注册（DeferredRegister）

`DeferredRegister<Registry, Entry>` 保存一个 `ClassRegistration` 或 `DataRegistration`；注册表每次发布 ready 事件时重放一次。参数必须可拷贝。

```cpp
static modapi::DeferredRegister<modapi::ItemRegistry, MyItem> gMyItem{
    modapi::ItemRegistry::builder<MyItem>("mymod:my_item").build()
};

void onServerStarted() {
    if (!gMyItem.isRegistered()) return;
    auto item = gMyItem.get(); // optional_ref<::Item>
}
```

- `isRegistered()`：至少完成过一次注册。
- `isReady()`：目标注册表是否已打开。
- `get()`：最近一次产物；炉子、酿造、分组等无单一产物时为空。
- `detach()`：移除监听；析构与服务器停止时自动执行。
- 不再提供 `operator*` / `operator->`。

## 物品（`ICustomItem<T>`）

```cpp
class MyItem : public modapi::item::ICustomItem<::Item> {
public:
    explicit MyItem(std::string const& identifier) : ICustomItem(identifier) {
        icon("mymod:my_item").maxStackSize(16).durability(100).glint();
    }
};

static modapi::DeferredRegister<modapi::item::ItemRegistry, MyItem> gMyItem{
    modapi::item::ItemRegistry::builder<MyItem>("mymod:my_item").build()
};
```

组件方法（都返回 `*this`，可链式）：

`icon`、`maxStackSize`、`durability`、`damage`、`displayName`、`rarity`、`tags`、`glint`、`handEquipped`、`offHandAllowed`、`canDestroyInCreative`、`enchantable`、`fuel`、`compostable`、`food`、`wearable`、`blockPlacer`、`cooldown`、`useModifiers`；任意其他组件用 `addComponent("minecraft:...", ptr)`。

物品描述直接使用原版类型：

```cpp
item.creativeCategory(::SharedTypes::v1_21_110::ItemCategory::CreativeItemCategory::Items);
item.creativeGroup("itemGroup.name.items");
item.description(...);      // SharedTypes::v1_21_110::ItemDescription
item.menuCategory(...);     // SharedTypes::v1_21_110::ItemMenuCategory
```

`initComponentItem(ctx)` 由注册表在分配 id 后调用，构造内层 `::ComponentItem` 并把描述写回 `Item` 的服务端字段。服务端行为不会自动代理到内层组件；需要时在自定义物品里 override 引擎虚函数并调用 `getComponentItem()->...`。

方块物品就是 `ICustomItem<::BlockItem>`：

```cpp
class MyBlockItem : public modapi::item::ICustomItem<::BlockItem> {
public:
    explicit MyBlockItem(std::string const& identifier)
    : ICustomItem(identifier, 0, ::HashedString{"mymod:my_block"}) {
        blockPlacer("mymod:my_block").maxStackSize(16);
    }
};
```

## 方块（`ICustomBlock<T>`）

```cpp
class MyBlock : public modapi::block::ICustomBlock<::BlockType> {
public:
    MyBlock(std::string const& identifier, int blockId)
    : ICustomBlock(identifier, blockId, ::BlockTypeRegistry::get()
          .lookupByName(::HashedString{"minecraft:stone"}, false)->mMaterial) {
        components(/* BlockComponentGroupDescription */);
        description(/* BlockDescription */);
        collisionBox({0, 0, 0}, {1, 1, 1});
        destructibleByMining(1.0f);
    }
};

static modapi::DeferredRegister<modapi::BlockRegistry, MyBlock> gMyBlock{
    modapi::block::BlockRegistry::builder<MyBlock>("mymod:my_block").build()
};
```

便利方法：`components()`、`addComponent()`、`blockTags()`、`collisionBox()`、`selectionBox()`、`destructibleByMining()`、`lightEmission()`、`displayName()`、`mapColor()`、`friction()`、`description()`、`menuCategory()`、`vanillaBlockData()`、`states()`、`permutations()`、`molangVersion()`。未列出的组件走 `addComponent("minecraft:...", ptr)`。

`BlockProperty` 是 `ICustomBlock` 的私有成员，不再作为注册参数；`getProperty()` 返回只读视图。

## 世界生成

特征：

```cpp
class MyFeature : public modapi::worldgen::ICustomFeature {
public:
    std::optional<BlockPos>
    place(modapi::worldgen::BlockHelper& helper, BlockPos const& pos, Random&) const override {
        if (!helper.isValidPosition(pos)) return std::nullopt;
        if (!helper.setBlock(pos, *stone)) return std::nullopt;
        return pos;
    }
};

static modapi::DeferredRegister<modapi::worldgen::FeatureRegistry, MyFeature> gFeature{
    modapi::worldgen::FeatureRegistry::builder<MyFeature>("mymod:my_feature").build()
};
```

规则把放置位置与放置内容分开；构造里设置 `placesFeature()` 与 `passes()`：

```cpp
class MyRule : public modapi::worldgen::ICustomFeatureRule {
public:
    MyRule() { placesFeature("mymod:my_feature").passes({"surface_pass"}); }

    ll::coro::Generator<BlockPos>
    place(modapi::worldgen::BlockHelper const&, BlockPos const& pos, Random&) override {
        co_yield pos;
    }
};

static modapi::DeferredRegister<modapi::worldgen::FeatureRuleRegistry, MyRule> gRule{
    modapi::worldgen::FeatureRuleRegistry::builder<MyRule>("mymod:my_rule").build()
};
```

`passes` 为空会被拒绝并记录 error。地表构建器：

```cpp
static modapi::DeferredRegister<modapi::worldgen::SurfaceBuilderRegistry, MyBuilder> gSurface{
    modapi::worldgen::SurfaceBuilderRegistry::builder<MyBuilder>("mymod:my_surface").build()
};
```

## 生物群系（`BiomeRegistry`）

```cpp
static modapi::DeferredRegister<modapi::worldgen::BiomeRegistry, modapi::worldgen::BiomeRegistration> gBiome{
    modapi::worldgen::BiomeRegistry::builder("mymod:my_biome")
        .property(modapi::worldgen::BiomeProperty{
            .mTemplate = "minecraft:plains",
            .mSurface  = std::string{"mymod:my_surface"},
        })
        .build()
};
```

`mTemplate` 必填；`mClimate` 是 `Replacement` 或 `BiomeClimate`；`mComponents` 每项是 `{mKey, mCreate}`；`mSurface` 是预设或已注册构建器名。注册必须在 `BiomeReadyEvent` 之前完成（`DeferredRegister` 已处理）。验证只需看 `/locate biome` 三个不同坐标、F3 和 `[BIOME]` 套件。

## 配方（`RecipeRegistry`）

普通配方是 concrete 类，可直接构造，也可用专用 builder：

```cpp
auto shaped = modapi::RecipeRegistry::getInstance().registerEntry<modapi::recipe::CustomShapedRecipe>(
    modapi::RecipeRegistry::builder<modapi::recipe::CustomShapedRecipe>("mymod:my_recipe")
        .shape({"X"})
        .ingredients({{"X", modapi::ICustomRecipe::Ingredient{"minecraft:stick"}}})
        .result(result)
        .build()
);
```

可用类：`CustomShapedRecipe`、`CustomShapelessRecipe`、`CustomShapedMultiRecipe`、`CustomShapelessMultiRecipe`、`CustomStoneCutterRecipe`、`CustomFurnaceRecipe`、`CustomBrewingRecipe`、`CustomSmithingTransformRecipe`、`CustomSmithingTrimRecipe`、`CustomJsonRecipe`。

用户自定义行为继承任意 `ICustom*Recipe`，用通用参数 builder：

```cpp
class MyRecipe : public modapi::ICustomShapedRecipe { /* ... */ };

static modapi::DeferredRegister<modapi::RecipeRegistry, MyRecipe> gRecipe{
    modapi::RecipeRegistry::builder<MyRecipe>("").build()
};
```

## 数据注册表

```cpp
// 战利品表
modapi::LootTableRegistry::builder("loot_tables/mymod/my_table.json").json(rawJson).build();

// 游戏规则
modapi::GameRuleRegistry::builder("mymod:my_rule").intDefault(7).shouldSaveToDisk(false).build();

// 创造栏分组
modapi::CreativeItemRegistry::builder("itemGroup.name.my_group")
    .category(::SharedTypes::CreativeItemCategory::Items)
    .icon(icon)
    .build();
```

## 资源包与行为包（RuntimePack）

`modapi::addons::RuntimePack` 在加载期安装资源包/行为包，支持临时目录和 zip 解包。服务器在 level 启动时组装包栈，因此必须在加载期安装。

## 测试

```bash
xmake build test
```

把 `bin/dll/ModAPI` 与 `bin/test/dll/test` 放到服务器的 `plugins/` 下，启动 `bedrock_server_mod.exe`。套件输出 `[REGISTRY]`、`[BLOCKS]`、`[BIOME]`、`[LOOTTABLE]`，报告写到 `plugins/test/*.txt`。

## 许可证

见 `LICENSE`。
