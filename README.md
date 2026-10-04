# ModAPI

ModAPI 是 GroupMountain 维护的 Minecraft Bedrock Dedicated Server 模组开发接口库，面向基于 LeviLamina 的服务器扩展开发。项目提供一组 C++20 API，用于注册和管理自定义物品、方块、配方、游戏规则、世界生成、战利品表、效果、实体等内容。

## 功能概览

- 自定义物品、护甲、工具和食物组件注册接口
- 自定义合成、熔炉、酿造、切石、锻造等配方接口
- 自定义游戏规则、世界生成特征、战利品表、状态效果和实体接口
- 每个注册表都会在原版注册时机发布「就绪事件」，可用 `DeferredRegister` 延迟注册
- Addons 加载支持（内置 zip 解包，不依赖 GMLIB）
- 基于 xmake 的构建流程，并在构建后生成 DLL、PDB、LIB、头文件和 `manifest.json`

## 环境要求

- Windows / Visual Studio C++ 工具链
- [xmake](https://xmake.io/)
- C++20 编译环境
- 依赖包：
  - LeviLamina `26.51.*`（`minizip-ng` 等其余包由 xmake 自动解析）

依赖仓库已在 `xmake.lua` 中配置：

- `https://github.com/LiteLDev/xmake-repo.git`

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
scripts/          构建辅助脚本
xmake.lua         xmake 构建配置
```

## 延迟注册（DeferredRegister）

注册表只在原版的注册时机接收条目（例如物品必须注入在 `VanillaItems::registerItems` 期间，晚于此点的物品不会进入主注册表）。`DeferredRegister` 把「一次注册」封装成一个用户构造的对象：模板参数给出目标注册表与自定义类型，构造参数就是该类型的构造参数，对象构造时即挂上该注册表的就绪事件监听。

```cpp
#include <modapi/DeferredRegister.h>
#include <modapi/item/ItemRegistry.h>

class MyItem : public modapi::ICustomItem {
public:
    MyItem(std::string const& identifier, int damage) : ICustomItem(identifier) { mMaxDamage = damage; }

    modapi::ItemIcon getIcon() const override { return modapi::ItemIcon{"my_texture"}; }
};

// 必须是 static/全局对象，且必须在注册表就绪之前构造（即模组加载期）
static modapi::DeferredRegister<modapi::ItemRegistry, MyItem> gMyItem("mymod:my_item", 100);

void onServerStarted() {
    if (!gMyItem.isRegistered()) return;        // 尚未完成注册
    optional_ref<::Item> item = gMyItem.get();  // 注册产物
    item->mMaxDamage;
    gMyItem->mMaxDamage;                        // 或经 operator-> 使用
}
```

`DeferredRegister` 的公开接口只有：

- 构造函数 `DeferredRegister(Args&&... args)`：保存构造参数、挂上 `T::EventType` 监听，并额外监听 `ServerStoppingEvent`，无重试、无懒加载；
- `isRegistered()`：条目是否已完成过注册（产物可用）；
- `isReady()`：目标注册表是否已就绪；
- `get()`：注册产物，类型为 `optional_ref<T::Product>`（如 `optional_ref<Item>`），未完成时为空；
- `operator*` / `operator->`：直接使用产物的便捷写法；
- `detach()`：移除本对象挂上的监听（析构函数会调用它，服务器停止时也自动调用一次；重复调用无害）。

监听器属于「代码在模组 DLL 里」的回调，**必须在该 DLL 被卸载前撤掉**，否则事件总线会在卸载后调用到已卸载模块。`DeferredRegister` 因此在 `ServerStoppingEvent`（两者都还活着）时自动 `detach()`，而不是拖到 DLL 析构。

其他说明：

- 原版注册时机按实例/按 level 重复发生（例如 `GameRules` 每次构造都会重填规则、每个 level 都会 `Recipes::init`），因此每次事件发布都会重新构造并注册条目，构造参数需要可拷贝。
- 只需要「时机」而不需要延迟注册时，仍可直接调用注册表 API；也可监听对应注册表的 `EventType`（如 `modapi::ItemReadyEvent`）。
- 未就绪时直接调用注册表 API 会记录错误并返回空产物。

## 世界生成

自定义特征通过 `place()` 拿到一个 `BlockHelper`，用来读写它所在区域的方块：

```cpp
class MyFeature : public modapi::ICustomFeature {
public:
    std::optional<BlockPos>
    place(modapi::BlockHelper& helper, BlockPos const& pos, Random& random) const override {
        if (!helper.isValidPosition(pos)) return std::nullopt;   // 区域外/高度范围外

        auto const& previous = helper.getBlock(pos);             // optional_ref<::Block const>
        if (!helper.setBlock(pos, *stone)) return std::nullopt;  // 失败会记日志，不抛异常

        return pos;
    }
};
```

要点：

- `BlockHelper` 是**轻量可拷贝**的句柄：拷贝/移动指向同一个区域，不分配内存；默认构造或包了空指针的句柄是「空句柄」，所有操作都会失败而不是崩溃。
- `setBlock` 返回 `bool`，位置非法（区域外、高度范围外、chunk 外）时返回 `false` 并记错误日志；`getBlock` 返回 `optional_ref<Block const>`。
- `Layer::Block` / `Layer::ExtraBlock` 分别对应方块本身与额外层（water logging、雪层等）；`getHeightRange()` / `getMinHeight()` / `getMaxHeight()` 给出所在维度的高度范围。

## 掉落表（LootTable）

`modapi::loot_table::LootTableRegistry` 挂在 `LootTables::lookupByName` 上 —— 这是原版**唯一**的掉落表查找入口（箱子内容、实体掉落、`LootTableReference` 嵌套引用都走它），所以模组注册的表整条链路可见，非模组目录原样交回引擎。

原版没有任何「注册表」API：它从资源包读取 `loot_tables/**.json`，用 `LootTable::deserialize` 建表，按目录缓存进 `LootTables::mLootTables`；生成走 `LootTable::fill` → `LootPool::addRandomItems`，掉落入口是 `LootResolver::getItemsFromKilling/Looting/Mining`。

四种注册方式（使用者都不必自己处理引擎细节）：

| 方式 | 接口 | 说明 |
| --- | --- | --- |
| JSON 文本 | `registerLootTableFromMemoryJson` / `registerLootTableFromJsonFile` | 交给引擎自己的 `LootTable::deserialize`，条目/条件/函数全部支持 |
| 引擎 DOM | `registerLootTableFromJsonValue` | 同上，文档在代码里组装 |
| 入口类型 | `registerEntry<Entry>` + `ICustomLootTable` | 与其它 `ICustom*` 同形：模组在 `_init()` 里造引擎表（`buildTable` 走引擎自己的反序列化），注册表只负责注册；可配 `DeferredRegister<LootTableRegistry, Entry>`，`_init()` 在引擎第一次查找时（`LootTableReadyEvent`）运行 |
| 代码构建 | `LootTableBuilder` | 物品池（权重/品质/数量）与对其它表的引用，不需要写 JSON 文本 |

在引擎第一次查找之前发起的注册会**排队**（`LootTable::deserialize` 要解析物品，不能在服务器刚启动时运行），在第一次查找时按顺序重放，所以那次查找本身就能拿到这些表；`hasPendingRegistrations()` 可查询是否还有排队项。

| 想要 | JSON 里 | 代码里 |
| --- | --- | --- |
| 自定义 `"type"`（entry 类型） | ✗ 引擎内部分派器只认 4 种（`LootItem`/`EmptyLootItem`/`LootTableEntry`/`LootTableReference`），服务端没有暴露 entry 的 `deserialize`（`LootItem`/`LootTableEntry`/`LootTableReference::deserialize` 都在 `#ifdef LL_PLAT_C` 里），没法注册新名字 | ✓ 继承 `LootPoolEntry`（`_createItem`/`getEntryType` 纯虚，`mWeight`/`mQuality`/`mConditions`/`mSubTable` 全 public），用 `LootTableBuilder::addEntry` 交进来 |
| 自定义 `"functions"` | ✗ 引擎的 `LootItemFunctions::deserialize`（`MCAPI`，能调 ✓）只认内置名字 | ✓ 继承 `LootItemFunction`（`getFunctionType`/`applyPreVersion` 纯虚，`mPredicates` public），加进 `LootItem::mFunctions` |
| 自定义 `"conditions"` | ✗ `LootItemCondition::deserialize` 是 `MCNAPI`（无法解析），且只认内置名字 | ✓ 继承 `LootItemCondition`（`getConditionType`/`_applies` 纯虚），加进条目或池的 `mConditions`、函数的 `mPredicates` |

真正执行掉落（把表跑出物品）：模组侧可用的入口是 `LootTableUtils::generateRandomDeathLoot(LootTable const&, Actor&, ActorDamageSource const*, ItemStack const*, Player*, float luck)` —— 静态 `MCAPI`，自己造 loot context，返回 `std::vector<ItemStack>` ✓。另一条看起来更顺手的路（`LootTableUtils::fillContainer` / `LootTable::fill`）需要 `Container&`，而**模组造不出 `Container`**：`Container::Container()` 未导出，且它带一个 `Bedrock::PubSub::Publisher` 成员（48 字节，构造/析构都不平凡），伪造它等于给自己埋雷。测试里用 `SimulatedPlayer::create(name, pos, dimension, rotation)`（`LLAPI`，服务端自带，无需客户端）拿一个虚拟玩家当实体，把注册的表跑成物品再断言（`src-test/LootTableTest.cpp` 的 `generate` 段）。
两点限制：这些枚举只有内置值（`EntryType` 四个、`FunctionType` 0–27），自定义实现只能返回最接近的那个；在代码里构造引擎掉落类型时，模组要像测试模组那样补几个引擎未导出的符号（`src/mc/ItemInstance.cpp`、`src/mc/StaticOptimizedString.cpp`、`src/modapi/semversion/SemVersion.cpp` —— 测试目标的 `xmake.lua` 就是这么接的）。若确实希望 JSON 里能写自定义名字，只能 hook `LootTable::deserialize`（`MCAPI`）或 `LootItemFunctions::deserialize`（`MCAPI`）做名字改写；这条「解析/生成侧事件」还没做。
`LootTableReadyEvent` 在引擎第一次查找掉落表时发布一次；`isReady()` 表示已发生过查找；`unregisterLootTable` 会连同引擎可能缓存的副本一起清掉；`getLootTable(dir)` 返回模组注册的表，或（查找发生后）原版的表。

服务端可自定义的函数（只用头文件里声明的非 `MCNAPI` 符号）：

| 函数 | 用途 |
| --- | --- |
| `LootTables::lookupByName` | 增/换/拦截整张表（本注册表已挂） |
| `LootTable::deserialize` | 引擎刚解析完就改（可批量改原版表） |
| `LootTable::fill` | 改箱子等容器的产出 |
| `LootPool::addRandomItems` | 改单个池的产出 |
| `LootResolver::getItemsFromKilling` / `getItemsFromLooting` / `getItemsFromMining` | 改实体/方块掉落 |
| `LootTableUtils::fillContainer` / `generateRandomDeathLoot` / `givePlayer` | 主动调用（也可 hook） |

不能用的：`LootPool::deserialize`、`LootItem`/`LootTableEntry`/`LootTableReference::deserialize`（仅客户端侧）、`LootItemCondition::deserialize`（`MCNAPI`，无法解析）。所以条件（conditions）在服务端只能走 JSON 路径产生，或自己继承 `LootItemCondition`；函数（functions）走 JSON 路径即可。

引擎的掉落类型在模组侧默认**链接不过**（例如 `RandomValueBounds` 的构造在 26.51 只对客户端声明），ModAPI 按 `src/mc/ItemInstance.cpp` 的老办法在 `src/mc/RandomValueBounds.cpp` 补齐了这几个符号（两个 float 用 `UntypedStorage::as<float>()` 写），`LootTableBuilder` 才能真正构造 `LootPool`/`LootItem`/`LootTableReference`。
## 方块（Block）

自定义方块**就是引擎的方块类型**：模组继承引擎的 `::BlockType`，ModAPI 用它自己的注册路径把这个类型交给引擎 —— 行为用 C++ 写，客户端需要的东西随注册一起给出。

### 行为用 C++

方块的**行为**来自 C++ 类（继承引擎的 `::BlockType`，和原版每个方块一个子类一样），**告诉客户端的东西**在注册时通过 `BlockProperty` 给出：

```cpp
class MyBlock : public ::BlockType {
public:
    MyBlock(std::string const& identifier, int id, ::Material const& material)
    : BlockType(identifier, id, material) {}
};

static modapi::DeferredRegister<modapi::BlockRegistry, MyBlock> gMyBlock{
    std::string{"mymod:my_block"},
    modapi::BlockRegistration<>{{}, modapi::BlockProperty{ /* 组件、状态、排列 */ }}
};
```

`BlockProperty` 完全由引擎自己的类型组成（`BlockComponentGroupDescription`、`BlockDescription`、`BlockPermutationDescription`），同一份属性同时服务两侧：组件被注入运行中的方块类型，并按引擎的联网格式发给客户端；方块状态与排列也随它一起发布。贴图、模型这类渲染数据仍由模组自己的资源包提供。

## 方块物品（`ICustomBlockItem`）

自定义方块要能被玩家拿到/放下，就需要一个**方块物品**。ModAPI 提供 `modapi::item::ICustomBlockItem`（继承引擎的 `::BlockItem` —— 放置行为就在它身上 ✓），并复用 `ItemRegistry` 的整套流程（`DeferredRegister` / `ItemReadyEvent` / 创造栏 / 命令枚举 ✓；`isEntry` 本来就是 `std::derived_from<Entry, ::Item>` ✓，所以不需要放宽 ✓）：

```cpp
class MyBlockItem : public modapi::item::ICustomBlockItem {
public:
    MyBlockItem(std::string const& identifier)
    : ICustomBlockItem(identifier, ::HashedString{"mymod:my_block"}) {}   // 也可传 ::BlockType&
};
static modapi::DeferredRegister<modapi::item::ItemRegistry, MyBlockItem> gMyBlockItem{std::string{"mymod:my_block"}};
```



需要让客户端拿到的东西（贴图、模型、音效、方块/物品定义、语言文件……）都可以在运行时打包下发，**不为某一种内容做特化**：

```cpp
modapi::addons::RuntimePack pack{"mymod_textures", "resources"};   // "data" 则是行为包
pack.addFile("blocks.json", R"({"format_version":[1,1,0],"mymod:thing":{"textures":"my_tex","sound":"stone"}})");
pack.addFile("textures/terrain_texture.json", terrainJson);
pack.addFileFrom("textures/blocks/my_tex.png", "C:/art/my_tex.png");   // 二进制直接拷贝
pack.install();
```


- 文件写在**系统 temp 下的一个随机目录**里（`modapi_packs_<随机>`，每个包一个 ✓），服务器目录与世界文件一概不动 ✓；
- `install()` 把该目录交给引擎自己的 addon 加载路径 —— `AddonsLoader::addCustomPackPath()` ✓：它为目录建立目录包源（`ResourcePackRepository` + `ResourcePackFactory::createDirectoryPackSource`，`PackOrigin::Test`）✓，再由 `ResourcePack::$ctor` / `ResourcePackStack::deserialize` 两个 hook 把这些包推进**客户端包栈** ✓。所以这不是 ModAPI 自己发明的一套，而是复用引擎加载行为包/资源包的那条路 ✓。
- 包也可以是 zip（`AddonsLoader` 会解压带 `manifest.json` 的压缩包 ✓）。
- `texturepack-required` 决定「拒绝资源包的客户端」能否进服 ✓。
- **必须在模组加载期安装** ✓：客户端拿到的包栈是在关卡启动时组好的 ✓，之后再装（比如从命令里）不会发给客户端 ✓（这一点正是「资源包好像没发送」最常见的原因 ✓）。

**没能验证的部分（如实写明）**：本机没有客户端 ✗，所以「真实客户端是否接受并渲染」无法验证 ✗；已验证的是：包按正确结构写在其随机 temp 目录里 ✓、`install()` 返回成功 ✓、引擎在服务端启动期间加载它们没有任何报错、关服退出码仍为 `0x0` ✓。
## 测试

`src-test/` 是一个独立的测试模组（xmake 目标 `test`，默认不构建），它把 ModAPI 作为依赖，在真实服务器上验证注册表 API：

```bash
xmake build test
```

产物在 `bin/test/`。把 `bin/test/dll/test` 和 `bin/dll/ModAPI` 一起放进服务器的 `plugins/` 目录后启动服务器，套件会在 `ServerStartedEvent` 时运行，结果写入 `plugins/test/` 下的 `*-test-report.txt` 并以 `[REGISTRY]` / `[BLOCKHELPER]` 前缀打进日志；也可以在游戏内用 `modapitest registry` / `modapitest blockhelper` 单独重跑某个套件。

当前两个套件：

- `[REGISTRY]`（122 条）：物品/创造栏/配方/游戏规则/世界生成特征的延迟注册与产物、就绪事件（每当事先挂上的监听都会收到并把注册表实例交出来）、加载期不应就绪的阶段前提、五个注册表的公开 API（物品查找与各 setter、创造分组与条目增删、各类配方与取消注册、游戏规则三种类型与取值、特征规则、`DeferredRegister` 的监听挂载/`detach()`/析构与停机监听、concept 约束）、以及 JSON 配方的拒绝路径。
- `[BLOCKHELPER]`（51 条）：句柄有效性与区域识别、高度范围与半开区间、越界与「chunk 之前一个方块」的拒绝、`Layer` 越界值、空/空指针句柄不崩溃、拷贝与移动语义、以及读写往返（含两种句柄互相可见性）。

测试可以用**虚拟玩家**（`SimulatedPlayer::create(name, pos, dimension, rotation)`，`LLAPI`，无需客户端）当「区块锚点」：创建它会加载周围区块，因此凡是依赖已加载区块的用例（`[BLOCKHELPER]` 的写入、以后的世界生成用例）都不再看运气 —— 它本身还是真 `Actor`/`Player`，能直接喂给引擎的掉落/行为接口。
读写方块的用例需要附近有已加载 chunk（无人的服务器不会加载任何 chunk），这类用例会记为 `skipped=` 而不是失败；`registerFeatureRule` 的特征只加入之后配置的 level，因此当前 level 的检查同样记为 `skipped=`。


符号约束：只有头文件里声明的非 `MCNAPI`（`MCAPI`/`LLAPI`/`LLNDAPI`）符号可用 —— 服务端导出表已剥离符号，`MCNAPI` 标记的函数无法解析。

测试模组同时是「如何编写基于 ModAPI 的模组」的参考实现：

- `src-test/MemoryOperators.cpp`：LeviLamina 只加载使用统一内存分配运算符的模组，任何模组都需要这样一份翻译单元（ModAPI 自己的在 `src/modapi/core/MemoryOperators.cpp`）；
- `src-test/RegistryTest.cpp`：注册物品、配方、游戏规则与世界生成特征，并断言注册产物、就绪事件与时机约束；
- `src-test/BlockHelperTest.cpp`：句柄有效性、区域识别、高度范围、越界与「chunk 前一个方块」的拒绝、拷贝/移动语义，以及读写的往返验证；
- `src-test/TestCommand.cpp`：命令系统只能在服务器启动后访问，注册类代码在模组加载期只能记录数据。

## 开发说明

- 公开接口头文件位于 `include/modapi/`。
- 构建前会自动执行头文件包含修正和格式化脚本。
- 版本信息由 `scripts/get-version-info.lua` 从最近的 git tag（`v<major>.<minor>.<patch>`）读取，注入到 `include/modapi/Version.h.in` 生成的配置头以及发布产物的 `manifest.json` 中。因此构建前需要先有对应 tag，未打 tag 时会沿用上一个已发布版本号。
- 发布产物的 `manifest.json` 由 `scripts/generate-manifest.lua` 生成。

## 许可证

本项目基于 GNU Affero General Public License v3.0 或更新版本（AGPL-3.0-or-later）开源。完整许可文本见 [LICENSE](LICENSE)。
