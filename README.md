# ModAPI

ModAPI 是 GroupMountain 维护的 Minecraft Bedrock Dedicated Server 模组开发接口库，面向基于 LeviLamina 的服务器扩展开发。它以 C++20 提供一组注册表接口，用于注册自定义物品、方块、配方、游戏规则、世界生成和战利品表。

注册表只在固定的注册时机接收条目，错过时机的条目不会生效。ModAPI 用每个注册表的 `EventType` 表示这些时机，`DeferredRegister` 会等到时机到达时自动注册条目；也可以直接监听事件，或在时机之内调用注册表 API。

## 功能概览

- 自定义物品：普通物品、方块物品、护甲、工具、食物
- 自定义方块：行为由 C++ 类给出，客户端需要的方块定义随注册一起提交
- 自定义配方：有序/无序（含多重配方）、熔炉、酿造、切石、锻造，以及 JSON 配方
- 自定义游戏规则（bool/int/float）与创造栏条目
- 自定义世界生成特征与特征规则
- 自定义战利品表：JSON 文本、引擎 DOM、入口类型、代码构建四条路径
- 运行时资源包/行为包（`modapi::addons::RuntimePack`，内置 zip 解包，不依赖 GMLIB）
- 基于 xmake 的构建流程，构建后生成 DLL、PDB、LIB、头文件和 `manifest.json`

## 环境要求

- Windows / Visual Studio C++ 工具链
- [xmake](https://xmake.io/)
- C++20 编译环境

## 构建

```bash
xmake f -m release
xmake
```

构建完成后，产物会复制到 `bin/` 目录，主要包括：

- `bin/dll/ModAPI/ModAPI.dll`
- `bin/dll/ModAPI/manifest.json`
- `bin/pdb/ModAPI.pdb`
- `bin/lib/ModAPI.lib`
- `bin/include/`

## 项目结构

```text
include/modapi/   对外公开的 ModAPI 头文件
src/modapi/       ModAPI 内部实现
src/mc/           Minecraft / LeviLamina 相关适配实现
src-test/         测试模组（xmake 目标 test，默认不构建）
scripts/          构建辅助脚本
xmake.lua         xmake 构建配置
```

## 延迟注册（DeferredRegister）

`DeferredRegister` 把「一次注册」封装成一个用户构造的对象：模板参数给出目标注册表与自定义类型，构造参数就是该类型的构造参数；该注册表就绪时，条目用这些参数构造并注册。

```cpp
#include <modapi/DeferredRegister.h>
#include <modapi/item/ItemRegistry.h>

class MyItem : public modapi::item::ICustomItem<::Item> {
public:
    using ICustomItem::ICustomItem;

    modapi::ItemIcon getIcon() const override { return modapi::ItemIcon{"my_texture"}; }
    short            getItemDurability() const override { return 100; }
};

// 必须是 static/全局对象，且必须在该注册表的第一次注册时机之前构造（即模组加载期）
static modapi::DeferredRegister<modapi::item::ItemRegistry, MyItem> gMyItem{std::string{"mymod:my_item"}};

void onServerStarted() {
    if (!gMyItem.isRegistered()) return;  // 尚未完成注册
    auto item = gMyItem.get();            // optional_ref<::Item>，注册产物
    gMyItem->mMaxStackSize;               // 或经 operator-> 使用
}
```

`DeferredRegister` 的公开接口：

- 构造函数 `DeferredRegister(Args&&... args)`：保存构造参数；注册表就绪时构造并注册条目。无重试、无懒加载。
- `isRegistered()`：条目是否已完成过注册（产物可用）。
- `isReady()`：目标注册表是否已就绪（转发到注册表的 `isReady()`）。
- `get()`：注册产物，类型为 `optional_ref<T::Product>`（如 `optional_ref<::Item>`），未完成时为空。炉子与酿造配方不产生 `::Recipe`，这类注册的产物为空。
- `operator*` / `operator->`：直接使用产物的便捷写法。
- `detach()`：提前注销本对象的延迟注册（析构时自动执行，服务器停止时也会执行一次；重复调用无害）。

其他说明：

- 构造参数需要可拷贝。
- 只需要「时机」而不需要延迟注册时，仍可直接调用注册表 API；也可监听对应注册表的 `EventType`（如 `modapi::item::ItemReadyEvent`）。
- 未就绪时直接调用注册表 API 会记录错误并返回空产物。

## 方块（Block）

自定义方块就是引擎的方块类型：模组继承引擎的 `::BlockType`，行为用 C++ 写；客户端需要的东西由注册时给出的 `BlockProperty` 描述，它完全由引擎自己的类型组成，组件、方块状态与排列随注册一起发布给客户端。贴图、模型这类渲染数据仍由模组自己的资源包提供。

```cpp
class MyBlock : public ::BlockType {
public:
    MyBlock(std::string const& identifier, int id, ::Material const& material)
    : BlockType(identifier, id, material) {}
};

static modapi::DeferredRegister<modapi::BlockRegistry, MyBlock> gMyBlock{
    std::string{"mymod:my_block"},
    modapi::BlockRegistration<>{{}, modapi::BlockProperty{/* 组件、状态、排列 */}}
};
```

`BlockProperty` 的字段：`mComponents`（`BlockComponentGroupDescription`，作为 `components` 发布）、`mDescription`（`BlockDescription`，发布 `menu_category`、`vanilla_block_data`、`traits`）、`mPermutations`、`mMolangVersion`（默认 12）与 `mExtra`（补充 NBT，最后合并进条目）。不提供属性的注册仍会得到一个由默认值构建的条目。

## 方块物品（`ICustomBlockItem`）

自定义方块要能被玩家拿到/放下，就需要一个方块物品。`modapi::item::ICustomBlockItem` 继承引擎的 `::BlockItem`，并复用 `ItemRegistry` 的整套流程（`DeferredRegister` / `ItemReadyEvent` / 创造栏 / 命令枚举）。

```cpp
class MyBlockItem : public modapi::item::ICustomBlockItem {
public:
    MyBlockItem(std::string const& identifier)
    : ICustomBlockItem(identifier, ::HashedString{"mymod:my_block"}) {}  // 也可传 ::BlockType const&
};
static modapi::DeferredRegister<modapi::item::ItemRegistry, MyBlockItem> gMyBlockItem{std::string{"mymod:my_block"}};
```

物品与方块按名字关联，物品图标由方块本身派生，无需另写图标。

## 世界生成（FeatureRegistry）

自定义特征通过 `place()` 拿到一个 `BlockHelper`，用来读写它所在区域的方块：

```cpp
class MyFeature : public modapi::worldgen::ICustomFeature {
public:
    std::optional<BlockPos>
    place(modapi::worldgen::BlockHelper& helper, BlockPos const& pos, Random& random) const override {
        if (!helper.isValidPosition(pos)) return std::nullopt;   // 区域外/高度范围外

        auto const& previous = helper.getBlock(pos);             // optional_ref<::Block const>
        if (!helper.setBlock(pos, *stone)) return std::nullopt;  // 失败会记日志，不抛异常

        return pos;
    }
};
```

要点：

- `setBlock` 返回 `bool`，位置非法时返回 `false`（并记错误日志）；`getBlock` 返回 `optional_ref<Block const>`。
- `Layer::Block` / `Layer::ExtraBlock` 分别读写方块本身与额外层（water logging、雪层等）。
- `isValidPosition(pos)` 判断位置是否在可写范围内；`getHeightRange()` / `getMinHeight()` / `getMaxHeight()` 给出所在维度的高度范围。

特征规则（`FeatureRuleRegistry`）把「放置位置」与「放置什么」分开：规则只产出位置，被引用的特征负责放置。

```cpp
class MyRule : public modapi::worldgen::ICustomFeatureRule {
public:
    ll::coro::Generator<BlockPos>
    place(modapi::worldgen::BlockHelper const& helper, BlockPos const& pos, Random& random) override {
        co_yield pos;
    }
};

static modapi::DeferredRegister<modapi::worldgen::FeatureRuleRegistry, MyRule> gMyRule{
    modapi::worldgen::FeatureRuleRegistration<>{
        .mIdentifier    = "mymod:my_rule",
        .mPlacesFeature = "mymod:my_feature",
        .mPasses        = {"mymod:trees"}
    }
};
```

`mPasses` 为空表示参与该 level 配置的全部 pass；规则在特征的注册时机之后注册，因此可以引用已注册的特征。

## 生物群系与地表构建器

群系的行为用 C++ 写：注册表条目、气候格、地表构建器都是代码。客户端看到的样子（贴图、语言文件）照旧通过
`RuntimePack` 提供，两者互不影响。

### 地表构建器（`SurfaceBuilderRegistry`）

一个构建器只实现 `buildSurfaceAt`，其余成员 `ICustomSurfaceBuilder` 已经给好（全部 inline，派生类不需要 ModAPI 的
任何符号）：

```cpp
class MySurface : public modapi::worldgen::ICustomSurfaceBuilder {
public:
    void buildSurfaceAt(
        ::ISurfaceBuilder::GlobalParameters const&,
        ::ISurfaceBuilder::SurfaceBuilderParameters const&,
        ::ISurfaceBuilder::PerColumnParameters const&      column
    ) const override {
        auto& volume = column.mBlocks;      // ::BlockVolume& —— 整个区块的暂存区，16 x 320 x 16
        auto  height = static_cast<size_t>(volume.mHeight);
        auto  depth  = static_cast<size_t>(volume.mDepth);
        auto  bottom = static_cast<int>(volume.mDimensionBottom); // 局部 y = 0 对应的世界 y

        auto& heights = *column.mPreWorldGenHeightmap.mData;      // 每列的地表高度（世界 y）
        auto  slot    = static_cast<size_t>((column.mPos.z & 15) * 16 + (column.mPos.x & 15));
        auto  top     = static_cast<int>(heights[slot]);

        auto localX = static_cast<size_t>(column.mPos.x & 15);
        auto localZ = static_cast<size_t>(column.mPos.z & 15);
        auto localY = static_cast<size_t>(top - bottom);
        auto index  = localY + height * (localZ + localX * depth); // y 变化最快

        volume.mBlocks.get().mBegin[index] = &someBlock;          // ::Block const*
    }
};
```

写入路径（引擎自己的 `BlockVolumeTarget::setBlock` 就是这么写的，`src-test/BiomeTest.cpp` 已实测到区块）：

- 索引是 `localY + mHeight * (localZ + localX * mDepth)`：**y 变化最快**，其次 z，最后 x；三项都取局部坐标，
  `localY = 世界 y - mDimensionBottom`。
- `mPos` 的 `y` 恒为 0，列坐标只有 `x`/`z` 有意义；`mPreWorldGenHeightmap` 是每列的地表高度（世界 y）。
- `mBlocks` 是**提交前的暂存区**：构建器返回后引擎把它写进区块，所以在构建器里读它没有意义，也不必自己提交。
- 引擎也会为**只做上下文**的邻居区块调用构建器，那些区块生成完就被丢掉，写入不会落盘。因此事后验证只能看
  `getExistingChunk` 返回非空、且底层方块已经生成的区块；`src-test/BiomeTest.cpp` 为每个区块记一个标记，再逐个试。

注册在 `SurfaceBuilderReadyEvent` 里——引擎的原版构建器已经就位、任何区块还没开始生成：

```cpp
ll::event::EventBus::getInstance().emplaceListener<modapi::worldgen::SurfaceBuilderReadyEvent>(
    [](auto& event) {
        event.registry().registerBuilder("mymod:my_surface", std::make_unique<MySurface>());
    }
);
```

- 引擎用 `isBestBuilder` 挑选构建器，而它只看得到 `SurfaceBuilderData`、分不清是哪个群系；因此
  `ICustomSurfaceBuilder::isBestBuilder` 恒为 `false`，永远不会参与这套挑选。ModAPI 改为在
  `buildChunkSurface` 之前按群系把注册的构建器装进去，只影响点名引用它的群系。
- 引擎的构建器表按 `SurfaceBuilderType` 编号，原版占用 `0..7`，ModAPI 从 `100` 起分配。
- 一个构建器可以被任意多个群系引用；名字重复的注册会被拒绝（引擎对同一个键保留先到的）。

### 群系（`BiomeRegistry`）

```cpp
static modapi::DeferredRegister<modapi::worldgen::BiomeRegistry, modapi::worldgen::BiomeRegistration> gMyBiome{
    modapi::worldgen::BiomeRegistration{
        .mIdentifier = "mymod:my_biome",
        .mProperty   = {
            .mTemplate = "minecraft:plains",                       // 必填：拿这个群系当模板
            .mClimate  = modapi::worldgen::BiomeProperty::Replacement{},   // 默认：接管模板的气候格
            .mSurface  = std::string{"mymod:my_surface"},          // 也可以是 SurfacePreset
        },
    }
};
```

`mTemplate` 必填：群系的组件（气候、地表材料、生成规则）没有一项能从 native mod 构造，只能复制一个已有群系作为模板。它**不是**引擎的 `BiomeReplacement`（`minecraft:replace_biomes`，那套是运行时按噪声替换群系）。

`mClimate` 是 `std::variant<Replacement, BiomeClimate>`：

- `Replacement` 与引擎的 `::BiomeReplacement` 字段一致（`mTargetBiomes` / `mDimension` /
  `mAmount` / `mNoiseFrequencyScale`），ModAPI 把它们映射到主世界多噪声格子上：目标群系的格子整份改判给本群系。
  当前只支持**整份替换**（`mAmount = 1`、默认噪声频率、`minecraft:overworld`），其余取值会直接报错拒绝，而不是静默当成整份替换。
- `BiomeClimate` 是自己的一格（归一化 `[-1, 1]`，内部换成引擎的定点数），由 ModAPI 追加进格子列表。

### 组件（`BiomeProperty::mComponents`）

引擎的群系组件存在 `BiomeComponentStorage` 里，按 `Bedrock::typeid_t<void>` 索引。这个 id 由引擎在静态初始化时分配，
**不导出**，所以 ModAPI 无法替你猜一个。接口是把 key 和工厂一起交上来：

```cpp
struct MarkerComponent : ::BiomeComponentBase { int mValue = 42; };

.mComponents = {
    {
        .mKey    = ::Bedrock::typeid_t<void>{60000},   // 见下：key 从哪来
        .mCreate = [] { return std::make_unique<MarkerComponent>(); },
    },
},
```

- `mCreate` 在每次注册时调用，重放的注册会拿到新对象，不会被两个群系共享。
- **引擎组件的 key**：`BiomeRegistry::dumpComponentKeys()` 会把每个群系现有的组件 key 全打出来（实测：`minecraft:plains`
  是 `0, 42, 43, 44, 51, 45, 46, 52, 50, 68, 69, 70`）。拿你要的组件在某个原版群系上的 key，填进来即可；同一次 BDS
  构建里 id 稳定。
- **ModAPI 自有组件**：随便挑一个远高于原版范围的 id（例如 `60000`），引擎从不查它，只有你自己的代码读；测试就是这么验的
  （`mymod:climate_biome` 的组件表末尾多出 `60000`）。
- 注入后 `mAllowAddingComponents` 会被置为 `true`；`applySerializedDataToBiome` 先跑，所以模板带来的组件（地表构建器数据等）
  仍在，自定义的追加在后面。

**格子是按「参数空间大小」选的**：引擎的 RTree 在包含该点的所有叶子中挑参数空间最大的那个。原版格子已经把整个空间铺满，所以比原版窄的自定义格子永远不会被选中，`BiomeClimate::fullRange()` 这类更宽的格子才会赢（实测：全范围格子 4225/4225 全中）。想「只替换一部分」就要靠 `Replacement` 的选择性接管。

**客户端外观**照旧走资源包：`modapi::addons::RuntimePack` 在模组加载时写包并交给引擎，客户端加入时就能拿到。
自定义群系能配的是数据，不是代码：

```
biomes_client.json                 # 每个群系的 water_surface_color / water_fog_color / fog_identifier
fogs/<name>.json                   # fog_identifier 指向的雾设置（air / water 的起止与颜色）
```

`src-test/BiomeTest.cpp` 的 `clientPack` 段就是这么做的：三个群系各配一套水色与雾，并断言包已写入、清单存在、
`biomes_client.json` 里三个名字都在。

### 进游戏怎么验（`src-test/BiomeTest.cpp` 的三个群系）

1. **命令**：`/locate biome mymod:cpp_biome`、`/locate biome mymod:cpp_biome2`、
   `/locate biome mymod:climate_biome` 各自返回**不同坐标**（`cpp_biome` 接管 plains、`cpp_biome2` 接管 forest、
   `climate_biome` 接管 savanna）。修复前三个会返回同一个坐标——那说明枚举里的值不对。
2. **站在里面**：`/tp` 到上面任一坐标，F3 的群系名显示对应的 `mymod:...`。
3. **雾**：客户端资源包给三个群系各配了一套雾——`cpp_biome` 偏红、`cpp_biome2` 偏蓝、`climate_biome` 偏绿，
   进出群系边界时空气雾会换色（`fogs/*.json` 里 air 的 fog_color，0.2–0.85 渲染距离）。
4. **服务端日志**：新区块生成时打印 `surface #N pos=(x,0,z) top=... local=(...) slot=.../81920 wrote gold at (x,y,z)`；
   `the /locate biome enum has 92 values` 是枚举被扩展的那一次。
5. **测试套件**：`BIOME RESULT passed=... failed=0`，其中 `clientPack` 段确认资源包已写入、三个群系名都在
   `biomes_client.json` 里；`engine` 段确认构建器被调用、且它写的方块能从事后的区块里读回来
   （`surfaceBuilder.called` / `surfaceBuilder.readBack`）。

`StoneSurfaceBuilder` 现在真的改地形：它在每列地表处写一个 `minecraft:gold_block`，几十 tick 后 `engine` 段从世界里
读回来（`readback #13/22 at (2960,63,2960) -> minecraft:gold_block (HIT)`）。进游戏同样能验：
`/execute if block <x> <y> <z> minecraft:gold_block run say HIT`；坐标所在的区块要在加载范围内，否则命令报
`out of range`。`mPreWorldGenHeightmap` 给的是地表高度，金块落在该列的地表那一格。


放置：主世界走接管/追加气候格；下界靠 `ReplaceBiomesComponent`；末地是固定群系源，目前不可用。

`/locate biome <名字>` 对自定义群系可用。引擎的命令枚举是一次性快照（在 `MinecraftCommands::initCoreEnums` 里建好），
所以 ModAPI 在服务器起来后从 `ll::service::getCommandRegistry()` 拿到注册表，用
`CommandRegistry::_addEnumValuesInternal` 把名字补进同一个枚举。不能手改 `mEnumValues`/`values`——符号带编码位
（`Symbol::toIndex()` 会掩掉高位），只塞字符串解析不了。

另外，引擎的 `BiomeRegistry::_register` 只建对象、**不写 `mId`**（那块内存是分配器残留），而 `/locate` 用的正是这个字段，
所以 `_registerBiome` 在注册后把它写成分配的 id；否则所有自定义群系会共享同一个残值、`/locate` 互相串。

id：从 `30000` 起，以引擎的 `mNextCustomBiomeId` 为准并逐个推进，行为包已经占掉的 id 会被跳过（实测包占了 30000，第一个模组群系拿到 30001）。

## 战利品表（LootTableRegistry）

模组注册的掉落表对整条取用链路可见：箱子内容、实体掉落与 `LootTableReference` 嵌套引用都能取到它；未注册的目录仍使用原版表。

注册只能在引擎第一次查找掉落表时进行，因此用 `DeferredRegister<LootTableRegistry, ...>` 等待 `LootTableReadyEvent`，在该事件中注册的表就是这次查找返回的表。`isReady()` 表示已发生过查找。

四种注册方式（使用者都不必自己处理引擎细节）：

| 方式 | 接口 | 说明 |
| --- | --- | --- |
| JSON 文本 | `registerLootTableFromMemoryJson` / `registerLootTableFromJsonFile` | 条目、条件、函数全部支持 |
| 引擎 DOM | `registerLootTableFromJsonValue` | 同上，文档在代码里组装 |
| 入口类型 | `registerEntry<Entry>` + `ICustomLootTable` | 与其它 `ICustom*` 同形：`_init()` 里造表（`buildTable` 或 `LootTableBuilder`）并给出目录，注册表只负责注册 |
| 代码构建 | `LootTableBuilder` | 物品池（权重/品质/数量）、对其它表的引用，以及自定义 `LootPoolEntry` |

其它公开接口：`getLootTable(dir)`（先返回模组注册的表，查找发生后可返回原版的表）、`unregisterLootTable(dir)`。

限制：JSON 路径只支持引擎内置的 entry/function/condition 类型，自定义 entry、function、condition 只能用代码构造 —— 继承 `LootPoolEntry` / `LootItemFunction` / `LootItemCondition`，用 `LootTableBuilder::addEntry` 或条目的 `mFunctions`/`mConditions` 交进来。类型枚举同样只有内置值，自定义实现只能返回最接近的那个。

## 资源包与行为包（RuntimePack）

需要让客户端拿到的东西（贴图、模型、音效、方块/物品定义、语言文件……）都可以在运行时打包下发，不为某一种内容做特化：

```cpp
modapi::addons::RuntimePack pack{"mymod_textures", "resources"};  // "data" 则是行为包
pack.addFile("blocks.json", R"({"format_version":[1,1,0],"mymod:thing":{"textures":"my_tex","sound":"stone"}})");
pack.addFile("textures/terrain_texture.json", terrainJson);
pack.addFileFrom("textures/blocks/my_tex.png", "C:/art/my_tex.png");  // 二进制直接拷贝
pack.install();
```

- `install()` 装载包并返回是否成功；装载后的包会下发给接入的客户端。带 `manifest.json` 的 zip 包同样支持。
- 包内容不写入服务器目录或世界文件。
- `texturepack-required` 决定「拒绝资源包的客户端」能否进服。
- **必须在模组加载期安装**：之后再安装（比如从命令里）不会下发给客户端。

## 测试

`src-test/` 是一个独立的测试模组（xmake 目标 `test`，默认不构建），它链接 ModAPI，在真实服务器上验证注册表 API：

```bash
xmake build test
```

产物在 `bin/test/`。把 `bin/test/dll/test` 和 `bin/dll/ModAPI` 一起放进服务器的 `plugins/` 目录后启动服务器，套件在服务器启动后运行，结果写入模组目录下的 `*-test-report.txt` 并以对应前缀打进日志；也可以在游戏内用 `modapitest <套件名>` 单独重跑某个套件。

| 套件 | 命令 | 覆盖范围 |
| --- | --- | --- |
| `[REGISTRY]` | `modapitest registry` | 物品/创造栏/配方/游戏规则/特征的延迟注册与产物、就绪事件、加载期不应就绪的阶段前提、五个注册表的公开 API、`DeferredRegister` 的注册与 `detach()`、concept 约束，以及 JSON 配方的拒绝路径 |
| `[BLOCKS]` | `modapitest blocks` | 方块的 C++ 类型注册与客户端属性发布、各类物品（普通/方块/食物/工具/护甲）的注册与查找、运行时资源包的安装 |
| `[BLOCKHELPER]` | `modapitest blockhelper` | 句柄有效性与区域识别、高度范围、越界与「chunk 之前一个方块」的拒绝、`Layer` 越界值、拷贝与移动语义、读写往返 |
| `[LOOTTABLE]` | `modapitest loottable` | 掉落表四条注册路径、覆盖原版表、主动生成掉落、取消注册与拒绝路径 |

依赖已加载 chunk 的用例（例如读写方块）在空跑的服务器上记为 `skipped=` 而不是失败。

## 开发说明

- 公开接口头文件位于 `include/modapi/`。
- 构建前会自动执行头文件包含修正与格式化脚本（`scripts/include_correction.py`、`scripts/format_all.py`）。
- 版本信息由 `scripts/get-version-info.lua` 从最近的 git tag（`v<major>.<minor>.<patch>`）读取，注入到 `include/modapi/Version.h.in` 生成的配置头以及发布产物的 `manifest.json` 中。因此构建前需要先有对应 tag，未打 tag 时会沿用上一个已发布版本号。
- 发布产物的 `manifest.json` 由 `scripts/generate-manifest.lua` 生成。

## 许可证

本项目基于 GNU Affero General Public License v3.0 或更新版本（AGPL-3.0-or-later）开源。完整许可文本见 [LICENSE](LICENSE)。
