# ModAPI

ModAPI 是 GroupMountain 维护的 Minecraft Bedrock Dedicated Server 模组开发接口库，面向基于 LeviLamina 与 GMLIB 的服务器扩展开发。项目提供一组 C++20 API，用于注册和管理自定义物品、方块、配方、游戏规则、世界生成、战利品表、效果、实体等内容。

## 功能概览

- 自定义物品、护甲、工具和食物组件注册接口
- 自定义合成、熔炉、酿造、切石、锻造等配方接口
- 自定义游戏规则、世界生成特征、战利品表、状态效果和实体接口
- Addons 加载支持
- 基于 xmake 的构建流程，并在构建后生成 DLL、PDB、LIB、头文件和 `manifest.json`

## 环境要求

- Windows / Visual Studio C++ 工具链
- [xmake](https://xmake.io/)
- C++20 编译环境
- 依赖包：
  - LeviLamina `26.10.*`
  - LeviBuildScript `0.6.0`
  - GMLIB `26.10.0`

依赖仓库已在 `xmake.lua` 中配置：

- `https://github.com/LiteLDev/xmake-repo.git`
- `https://github.com/GroupMountain/xmake-repo.git`

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

## 开发说明

- 公开接口头文件位于 `include/modapi/`。
- 构建前会自动执行头文件包含修正和格式化脚本。
- 版本信息由 `scripts/get-version-info.lua` 读取并注入到 `include/modapi/Version.h.in` 生成的配置头中。
- 发布产物的 `manifest.json` 由 `scripts/generate-manifest.lua` 生成。

## 许可证

本项目基于 GNU Affero General Public License v3.0 或更新版本（AGPL-3.0-or-later）开源。完整许可文本见 [LICENSE](LICENSE)。
