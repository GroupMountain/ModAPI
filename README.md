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
