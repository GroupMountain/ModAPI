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

迁移对照（旧的注册宏已移除）：

```cpp
// 旧写法
MODAPI_REGISTER_ITEM(MyItem, "mymod:my_item", 100);

// 新写法
static modapi::DeferredRegister<modapi::ItemRegistry, MyItem> gMyItem("mymod:my_item", 100);
```

自定义特征（feature）以标识符作为第一个构造参数：

```cpp
static modapi::DeferredRegister<modapi::FeatureRegistry, MyFeature> gFeature("mymod:my_feature", ctorArgs...);
```

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

原版没有任何「注册表」API：它从资源包读取 `loot_tables/**.json`，用 `LootTable::deserialize` 建表，按目录缓存进 `LootTables::mLootTables`；生成走 `LootTable::fill` → `LootPool::addRandomItems`，掉落入口是 `LootResolver::getItemsFromKilling/Looting/Mining`。上面的地址与调用关系用旧版 IDA（`1.26.30.1-mcedu`）核对过：`lookupByName` @0x10b688e50、`fill` @0x10b68a8e0、`addRandomItems` @0x10b688360、`deserialize` @0x10b68d840（旧版两参，新版三参，实现以新版头文件为准）。

四种注册方式（使用者都不必自己处理引擎细节）：

| 方式 | 接口 | 说明 |
| --- | --- | --- |
| JSON 文本 | `registerLootTableFromMemoryJson` / `registerLootTableFromJsonFile` | 交给引擎自己的 `LootTable::deserialize`，条目/条件/函数全部支持 |
| 引擎 DOM | `registerLootTableFromJsonValue` | 同上，文档在代码里组装 |
| 入口类型 | `registerEntry<Entry>` + `ICustomLootTable` | 与其它 `ICustom*` 同形：模组在 `_init()` 里造引擎表（`buildTable` 走引擎自己的反序列化），注册表只负责注册；可配 `DeferredRegister<LootTableRegistry, Entry>`，`_init()` 在引擎第一次查找时（`LootTableReadyEvent`）运行 |
| 代码构建 | `LootTableBuilder` | 物品池（权重/品质/数量）与对其它表的引用，不需要写 JSON 文本 |

在引擎第一次查找之前发起的注册会**排队**（`LootTable::deserialize` 要解析物品，不能在服务器刚启动时运行），在第一次查找时按顺序重放，所以那次查找本身就能拿到这些表；`hasPendingRegistrations()` 可查询是否还有排队项。文档解析用的格式版本取自引擎自己的常量 `SharedConstants::CurrentGameSemVersion()`（`MCAPI`）——`SemVersion` 持有打包指针，用 `fromString` 解析出来的那份是「拥有型」，析构/返回拷贝时会去释放不属于它的指针（实测在 `mi_free` 崩溃），从常量构造的是静态指向，安全。注册只可能在引擎就绪后进行（早于第一次查找的调用会报错并指向 `DeferredRegister`，因为 `LootTable::deserialize` 要解析物品）；`unregisterLootTable` 只摘掉本注册表拥有的表，**不碰** `LootTables::mLootTables` 这份引擎自己的缓存（那里永远是引擎自己加载的表，抹掉它会让正在使用该指针的人拿到悬垂指针）。`entries` 里的东西能不能自定义：

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

自定义方块**就是引擎的方块类型**：模组继承 `modapi::block::ICustomBlock`（它本身是 `::BlockType`，和原版给每个方块单独一个子类一样），ModAPI 用原版自己的注册路径把它交给引擎：

```cpp
class MyBlock : public modapi::block::ICustomBlock {
public:
    MyBlock(std::string const& identifier, int id)
    : ICustomBlock(identifier, id, ::BlockTypeRegistry::get().lookupByName(::HashedString{"minecraft:stone"}, false)->mMaterial) {}
};
static modapi::DeferredRegister<modapi::BlockRegistry, MyBlock> gMyBlock{std::string{"mymod:my_block"}};
```

实现要点（都经过实测）：

| 项 | 事实 |
| --- | --- |
| 挂载点 | `Level::loadBlockDefinitionGroup`（虚函数 → hook `$loadBlockDefinitionGroup` thunk，头文件里是 `MCAPI`）→ 之后发布 `BlockReadyEvent` ✓ |
| 注册方式 | `BlockTypeRegistry::registerBlock<Entry>(name, id, args...)`（头文件内联模板，要求 `Entry` 派生自 `BlockType` —— 这也是必须继承而不是手搓 `BlockDefinition` 的原因） |
| id | 取自 `BlockDefinitionGroup::mLastBlockId` 递增，实测自定义 id 从 **10001** 起（引擎给「非基础游戏」方块留的区间） |
| 状态生成时机 | **不是注册时**：`prepareBlocks`/`finalizeBlockComponentStorage` 在 `VanillaWorldSystems::init` 里稍后执行，注册当下 `getDefaultBlockState` 还是空 ✗ → 所以断言（能否放置）必须放在 `ServerStartedEvent` 之后 ✓ |
| 客户端 | 引擎靠 `BlockDefinitionGroup::generateServerBlockProperties()`（`ServerBlockProperty` = 名字 + `CompoundTag`）→ `StartGamePacket` 同步方块属性 ✓；自定义方块的定义来自上面那份 JSON ✓，所以该函数的结果里包含它（测试断言 ✓）。纹理仍来自客户端资源包 —— 与 addon 方块完全一致 ✓；“真实客户端是否接受”需要实机客户端确认（本机没有客户端，未验证 ✗） |

### 行为用 C++，定义用 JSON

方块的**行为**来自 C++ 类（`ICustomBlock : ::BlockType`，和原版每个方块一个子类一样），**定义**（客户端解析的东西：`minecraft:material_instances`、几何……）来自一份 addon 风格文档：

```cpp
class MyBlock : public modapi::block::ICustomBlock {
public:
    MyBlock(std::string const& identifier, int id, ::Material const& material)
    : ICustomBlock(identifier, id, material) {}
};
static modapi::DeferredRegister<modapi::BlockRegistry, MyBlock> gMyBlock{std::string{"mymod:my_block"}};

// 在模组加载期调用（引擎是在关卡加载方块定义时读它们的）
modapi::block::registerBlockFromJson(R"({
  "format_version": "1.21.0",
  "minecraft:block": {
    "description": { "identifier": "mymod:my_block" },
    "components": { "minecraft:material_instances": { "*": { "texture": "my_tex", "render_method": "opaque" } } }
  }
})");
```

实现与实测：

| 项 | 事实 |
| --- | --- |
| 文档怎么进引擎 | `registerBlockFromJson` 把它作为**行为包**（`modapi::addons::RuntimePack` → `AddonsLoader`）装进随机 temp 目录 ✓，引擎在自己的定义 pass 里解析它 ✓（实测日志：`probe4`/99 条定义 ✓）。`BlockDefinitionGroup::loadResource`/`_parseComponents` 需要 `PackLoadContext&`，而它的默认构造**没有导出**（LNK2019 ✗）→ 所以不走自己造 context，而是走引擎的 addon 路径 ✓。 |
| 同名怎么办 | 引擎解析完还会**自己注册一个同名方块类型** ✗，这就是「行为包 + 代码注册同名」**崩服（0xC0000005，两次复现）** 的原因：`BlockTypeRegistry::registerBlock` 用 `try_emplace`，名字已存在时**静默丢弃**我们 new 的对象 ✓，两边对象不一致后踩空 ✓。现在 ModAPI hook 掉 `BlockDefinitionGroup::registerBlockFromDefinition` ✓：对自己带进来的文档**跳过引擎的类型注册**、只保留解析好的定义 ✓，于是 C++ 类型就是引擎认的那个 ✓（测试断言 `jsonDefinition.cppObjectIsTheRegisteredType` 用指针相等证明 ✓）——同样组合现在 `exit=0x0` ✓。 |
| 撞车防护 | 若某个 identifier 已被**不受 ModAPI 管**的行为包定义 ✓，代码再注册同名方块会被**拒绝并报错** ✓（而不是留下不一致状态或崩服 ✓）。 |
| 客户端 | 定义进了定义组 ✓，`generateServerBlockProperties()` 里就有这个方块 ✓（断言 ✓）。材质实例不是「网络组件」✓，所以渲染数据仍由客户端资源包提供 ✓（`RuntimePack` 那条通用路线 ✓）。 |
| 已验证/未验证 | 已验证：JSON 定义被引擎解析 ✓、C++ 对象是注册类型 ✓、材质组件出现在定义里 ✓、四套件全绿 ✓、关服 `0x0` ✓。未验证：真实客户端是否接受/渲染 ✗（本机没有客户端 ✓）。 |

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

实测要点：

| 项 | 事实 |
| --- | --- |
| 物品→方块的关联 | **不是** `getBlockTypeForRendering()` ✗（`::BlockItem` 根本不覆写它 ✓），而是 **`::Item::mBlockType`** ✓。`BlockItem` 构造时不会替你设上 ✓ → `ICustomBlockItem::_init()` 自己查一次写进去 ✓。修之前物品渲染成 `minecraft:air` ✓，修之后是 `modapi_test:test_block` ✓。 |
| 注册时机 | 方块**类型**在 `BlockReadyEvent`（= level 读完定义之后 ✓）注册就够 ✓ —— 实测 level 读定义在 **04.384**、原版构建物品在 **04.777** ✓，方块物品查得到 ✓。（我曾临时把它挪到物品 pass 开头 ✗，实测证明没必要 ✓，已回滚 ✓；id 也回到引擎的 `BlockDefinitionGroup::mLastBlockId`（10001+ ✓）。） |
| 覆盖生效方式 | 物品侧 `initCustomItem(ICustomBlockItem&)`（与 `ICustomItem`/`ICustomArmorItem` 同款 ✓）把模组重载的值写进引擎字段 ✓：实测 `stack = 16, glint = true` ✓；引擎虚函数覆写（如 `isMusicDisk()` ✓）与方块侧覆写（如 `canProvideSupport` ✓）都有断言 ✓。 |
| 客户端定义（创造栏图标/名字） | 方块物品**必须自己给客户端一份定义** ✓：`NetworkTagBuilder` 现在有 `buildClientComponents(ICustomBlockItem const&)` 重载 ✓，`ICustomBlockItem::buildNetworkTag()` 调它 ✓。模组用 `getIcon()` 给贴图名（`::modapi::ItemIcon{"my_texture"}` ✓，由资源包的 `terrain_texture.json` 映射 ✓），用 `getDisplayName()` 给显示名（留空则走语言文件 ✓，资源包里放 `texts/en_US.lang` 的 `item.<ns>.<name>.name=…` ✓）。实测：不补这条时创造栏那格只有形状没有贴图 ✗，补上后物品定义里带上贴图名 ✓（断言 `blockItem.tagHasIcon` ✓）。 |
| 物品与方块同名 | 建议像原版那样让方块物品**与方块同名** ✓（`minecraft:stone` 既是方块也是物品 ✓）——引擎的「方块→物品」解析依赖它 ✓，这也是中键选取方块能拿到物品的关键 ✓。 |
| 已知问题（既有，不属本次范围） | 普通 `ICustomItem` 的引擎字段会被重置为默认值 ✓：实测引擎注册表里那个对象**不是** ModAPI 建的那个（`0x…900` vs `0x…400` ✓），data-driven 物品在后续 pass 里被引擎重建 ✓ —— 测试里只记录该观测值、不断言 ✓，详见 CHANGELOG ✓。另外本构建**没有 RTTI 数据** ✗（`dynamic_cast` 会以 "no RTTI data" 中止 ✓），取回具体类型请用你手上的指针 + `static_cast` ✓。 |
## 运行时客户端内容（`RuntimePack`）

需要让客户端拿到的东西（贴图、模型、音效、方块/物品定义、语言文件……）都可以在运行时打包下发，**不为某一种内容做特化**：

```cpp
modapi::addons::RuntimePack pack{"mymod_textures", "resources"};   // "data" 则是行为包
pack.addFile("blocks.json", R"({"format_version":[1,1,0],"mymod:thing":{"textures":"my_tex","sound":"stone"}})");
pack.addFile("textures/terrain_texture.json", terrainJson);
pack.addFileFrom("textures/blocks/my_tex.png", "C:/art/my_tex.png");   // 二进制直接拷贝
pack.install();
```

工作机制（全部实测）：

- 文件写在**系统 temp 下的一个随机目录**里（`modapi_packs_<随机>`，每个包一个 ✓），服务器目录与世界文件一概不动 ✓；
- `install()` 把该目录交给引擎自己的 addon 加载路径 —— `AddonsLoader::addCustomPackPath()` ✓：它为目录建立目录包源（`ResourcePackRepository` + `ResourcePackFactory::createDirectoryPackSource`，`PackOrigin::Test`）✓，再由 `ResourcePack::$ctor` / `ResourcePackStack::deserialize` 两个 hook 把这些包推进**客户端包栈** ✓。所以这不是 ModAPI 自己发明的一套，而是复用引擎加载行为包/资源包的那条路 ✓。
- 包也可以是 zip（`AddonsLoader` 会解压带 `manifest.json` 的压缩包 ✓）。
- 二进制文件用 `addFileFrom(包内路径, 磁盘路径)` 直接拷 ✓ —— 贴图必须是**真的 PNG** ✓（测试里嵌了一张 16×16 的棋盘格 PNG ✓，`src-test/assets/modapi_test_block.png` ✓；随手写一个假文件客户端是画不出来的 ✗，这一点我踩过一次 ✓）。
- `texturepack-required` 决定「拒绝资源包的客户端」能否进服 ✓。
- **uuid 每次都不同**：包的 header/module uuid 随机生成 ✓（实测同名两个包拿到不同 uuid ✓，日志会打出 uuid 便于跨次运行对比 ✓）。原因是客户端若已经装了同 uuid 的包就不会再下载 ✓——每次运行都是新包内容，必须给它一个没见过的身份 ✓。
- **必须在模组加载期安装** ✓：客户端拿到的包栈是在关卡启动时组好的 ✓，之后再装（比如从命令里）不会发给客户端 ✓（这一点正是「资源包好像没发送」最常见的原因 ✓）。`server4` 实测日志可直接核对：包的 `RuntimePack[...]: installed` 行出现在 `this level loaded N block definitions` 之前 ✓。

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

测试可以用**虚拟玩家**（`SimulatedPlayer::create(name, pos, dimension, rotation)`，`LLAPI`，无需客户端）当「区块锚点」：创建它会加载周围区块，因此凡是依赖已加载区块的用例（`[BLOCKHELPER]` 的写入、以后的世界生成用例）都不再看运气 —— 它本身还是真 `Actor`/`Player`，能直接喂给引擎的掉落/行为接口。用 RAII 保证 `remove()`（断言抛异常也会移除），实测不会写玩家存档、关服退出码仍为 `0x0`。
读写方块的用例需要附近有已加载 chunk（无人的服务器不会加载任何 chunk），这类用例会记为 `skipped=` 而不是失败；`registerFeatureRule` 的特征只加入之后配置的 level，因此当前 level 的检查同样记为 `skipped=`。

已修复：**给自定义物品设置 `mCreativeCategory` 后，关服时引擎会以 `0xC0000005` 结束**。用 r2 分析新版服务端（`bedrock_server_mod.exe 1.26.51+0559ac5`，基址 `0x140000000`）后定位到 RVA `0x168247A`：

```asm
lea rax, [0x14a64c970]    ; Bedrock::EnableNonOwnerReferences 的 vftable
mov [rsi], rax
mov rax, [rsi + 8]        ; mNonOwnerReferences（shared_ptr 的数据指针，this+0/8/0x10 = vftable/ptr/ctrl）
mov byte [rax], 0         ; ← rax = 0，空指针写
mov rsi, [rsi + 0x10]     ; 再释放控制块
```

也就是说：某个 `EnableNonOwnerReferences` 派生对象析构时，它的「非拥有引用表」指针是 null，而析构函数照样去写表里的标志位。该析构函数释放的成员偏移（`+0x18` 的 `std::string`、`+0x50`/`+0x60` 的分组映射、`+0x88` 的索引 vector 及其 `+0x98` 容量）与 `CreativeItemGroupCategory` 的布局逐项吻合，调用它的正是 `LeviLamina.dll` 的 `ll::service::bedrock::PropertiesSettingsInit::detour`（`TargetedBedrock.cpp:47`）。

根因：`registerCreativeGroup` 以前用 `try_emplace` **自己造分类对象**，这样造出来的 `CreativeItemGroupCategory` 基类从未被引擎登记进非拥有引用表，于是关服时引擎一析构就写到 null。现在改为使用引擎自己的 `createCategories()`（头文件声明的 `MCAPI`，可解析）返回的分类对象；另外两处也按引擎的路径补齐：新条目通过 `CreativeGroupInfo::_addCreativeItemEntry`（`MCAPI`）记入所属分组，条目不再往引擎自己的 `mCreativeNetIdIndex` 里塞伪造项。

符号约束（重要）：新版服务端 exe 已剥离符号（导出表仅 `0xf5` 字节），**`MCNAPI` 标记的函数无法解析**；只有头文件里声明的非 `MCNAPI`（`MCAPI`/`LLAPI`/`LLNDAPI`）符号可用。测试里的 `creative.addEntrySymbolResolved` 固化了这一点。验证：`server4` 上 `[REGISTRY] passed=123 failed=0 skipped=1`、`[BLOCKHELPER] passed=51 failed=0`，关服退出码 `0x0`（不再产生 `logs/crash/*`）。

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
