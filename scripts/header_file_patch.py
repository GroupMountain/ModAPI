import sys
import os
import re
from typing import NoReturn, Callable, List

def info(message: str) -> None:
    print(f"[{os.path.basename(__file__)}] \033[1;32m[INFO]\033[0m {message}")

def fatal(message: str) -> NoReturn:
    print(f"[{os.path.basename(__file__)}] \033[1;31m[FATAL]\033[0m {message}")
    sys.exit(1)

class HeaderPatcher:
    """通用的头文件修补器"""

    def __init__(self, source_path: str, target_path: str):
        self.source_path = source_path
        self.target_path = target_path
        self.content = ""
        self._modifications: List[Callable[[str], str]] = []

    def __enter__(self) -> 'HeaderPatcher':
        """进入上下文，读取源文件"""
        if not os.path.exists(self.source_path):
            fatal(f"Header file '{self.source_path}' does not exist.")

        with open(self.source_path, 'r', encoding='utf-8') as file:
            self.content = file.read()

        return self

    def __exit__(self, exc_type, exc_val, exc_tb) -> None:
        """退出上下文，保存修改后的文件"""
        if exc_type is None:
            # 应用所有修改
            for modification in self._modifications:
                self.content = modification(self.content)

            # 确保目标目录存在
            os.makedirs(os.path.dirname(self.target_path), exist_ok=True)

            # 写入目标文件
            with open(self.target_path, 'w', encoding='utf-8') as file:
                file.write(self.content)

            info(f"Patched header file saved to '{self.target_path}'.")

    def with_modification(self, modification: Callable[[str], str]) -> 'HeaderPatcher':
        """添加修改函数，返回自身以支持链式调用"""
        self._modifications.append(modification)
        return self

    def find_and_insert(self, pattern: str, insert_content: str, after: bool = True, occurrence: int = 1) -> 'HeaderPatcher':
        """查找模式并在其前后插入内容"""
        def modification(content: str) -> str:
            position = -1
            for i in range(occurrence):
                position = content.find(pattern, position + 1)
                if position == -1:
                    fatal(f"Failed to find pattern '{pattern}' (occurrence {i + 1}) in header file.")

            if after:
                insert_position = position + len(pattern)
                return content[:insert_position] + insert_content + content[insert_position:]
            return content[:position] + insert_content + content[position:]

        return self.with_modification(modification)

    def find_and_replace(self, pattern: str, replacement: str, count: int = 0) -> 'HeaderPatcher':
        """查找并替换模式"""
        def modification(content: str) -> str:
            if count == 0:
                return content.replace(pattern, replacement)
            return content.replace(pattern, replacement, count)

        return self.with_modification(modification)

    def regex_replace(self, pattern: str, replacement: str, flags: int = 0) -> 'HeaderPatcher':
        """使用正则表达式替换"""
        def modification(content: str) -> str:
            return re.sub(pattern, replacement, content, flags=flags)

        return self.with_modification(modification)

    def insert_at_beginning(self, content_to_insert: str) -> 'HeaderPatcher':
        """在文件开头插入内容"""
        def modification(content: str) -> str:
            return content_to_insert + content

        return self.with_modification(modification)

    def insert_at_end(self, content_to_insert: str) -> 'HeaderPatcher':
        """在文件结尾插入内容（在最后一个字符前）"""
        def modification(content: str) -> str:
            return content + content_to_insert

        return self.with_modification(modification)

class HeaderPatchManager:
    """头文件修补管理器"""

    @staticmethod
    def patch_humanoid_armor_item_header(project_path: str, levilamina_path: str) -> None:
        """修补 HumanoidArmorItem.h """
        with HeaderPatcher(
            os.path.join(levilamina_path, "mc", "world", "item", "HumanoidArmorItem.h"),
            os.path.join(project_path, "include", "modapi", "item", "types", "mc", "HumanoidArmorItem.h")
        ) as patcher:
            def add_init_functions(content: str) -> str | NoReturn:
                """添加初始化函数"""
                if "$initServer" in content and "$initClient" in content:
                    fatal("The header file already contains '$initServer' and '$initClient'. Please update the patch script accordingly.")
                insert_position = content.rfind("};")
                if insert_position == -1:
                    fatal("Failed to find '};' in header file.")

                init_functions = """
// tmpe fix
MOD_API PuvLoadData::LoadResultWithTiming initServer(Json::Value const& json, SemVersion const& version, IPackLoadContext& context, JsonBetaState const state) override;
MOD_API PuvLoadData::LoadResultWithTiming initClient(Json::Value const& json, SemVersion const& version, JsonBetaState const state, IPackLoadContext& context) override;"""
                return content[:insert_position] + init_functions + content[insert_position:]
            (patcher
                .find_and_insert(
                    "#pragma once\n\n",
                    "#include \"modapi/Macros.h\"\n"
                )
                .find_and_insert(
                    "prevent constructor by default", 
                    "\nMOD_NDAPI HumanoidArmorItem(std::string const& name, HumanoidArmorItem::Tier armorTier);",
                    occurrence=1
                )
                .with_modification(
                    add_init_functions
                )
                .regex_replace(
                    r'::ll::TypedStorage<\d+, \d+, (.*)>',
                    lambda match: match.group(1),
                    re.MULTILINE
                )
                .regex_replace(
                    r'([a-zA-Z0-9-_]+)\[(\d+)\]\s+([a-zA-Z0-9-_]+)', 
                    lambda match: f'{match.group(1)} {match.group(3)}[{match.group(2)}]',
                    re.MULTILINE
                )
                .regex_replace(
                    r'([a-zA-Z0-9:]+)\s+const\s+(.*;)', 
                    lambda match: f'{match.group(1)} {match.group(2)}',
                    re.MULTILINE
                )
            )

def main(project_path: str, levilamina_path: str) -> None:
    """主函数"""
    HeaderPatchManager.patch_humanoid_armor_item_header(project_path, levilamina_path)

if __name__ == "__main__":
    if len(sys.argv) < 3:
        fatal("Usage: python header_file_patch.py <project_path> <levilamina_path>")
    if not os.path.exists(sys.argv[1]):
        fatal(f"Path '{sys.argv[1]}' does not exist.")
    if not os.path.exists(sys.argv[2]):
        fatal(f"Path '{sys.argv[2]}' does not exist.")
    main(*sys.argv[1:3])
