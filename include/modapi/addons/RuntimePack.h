#pragma once
#include "modapi/Macros.h"
#include <filesystem>
#include <memory>
#include <string>

namespace modapi::inline addons {

// A pack built at runtime and handed to the engine, for any content that has to reach a client: textures,
// models, sounds, block or item definitions, ...
//
// The files are written into a fresh directory under the system temp directory (one per pack, so two packs
// never collide and nothing of the user's is touched), and the directory is registered with
// `AddonsLoader`, which gives it to the engine as a directory pack source. The engine then loads it like
// any addon pack and puts it into the pack stack that joining clients are told about.
//
//   modapi::addons::RuntimePack pack{"my_textures", "resources"};
//   pack.addFile("blocks.json", R"({"format_version": [1,1,0], "mymod:block": {"textures": "my_tex"}})");
//   pack.addFile("textures/terrain_texture.json", terrainJson);
//   pack.addFileFrom("textures/blocks/my_tex.png", "C:/my/art/my_tex.png");
//   pack.install();
//
// `moduleType` is the module inside the manifest: "resources" for a resource pack, "data" for a behavior
// pack (which is where clients read block definitions from).
class RuntimePack {
public:
    MOD_API explicit RuntimePack(std::string name, std::string moduleType = "resources");
    MOD_API ~RuntimePack();

    RuntimePack(RuntimePack const&)            = delete;
    RuntimePack(RuntimePack&&)                 = delete;
    RuntimePack& operator=(RuntimePack const&) = delete;

    // `relativePath` is where the file goes inside the pack; the directories are created as needed.
    MOD_API RuntimePack& addFile(std::filesystem::path const& relativePath, std::string const& content);
    // Copies a file that already exists on disk into the pack (a texture, a sound, ...).
    MOD_API RuntimePack& addFileFrom(std::filesystem::path const& relativePath, std::filesystem::path const& source);

    // Writes the pack and asks the engine to load it. Returns false when nothing could be written; the
    // directory it ended up in is `root()`.
    MOD_API bool install();

    [[nodiscard]] MOD_API std::filesystem::path const& root() const;

private:
    struct Impl;
    std::unique_ptr<Impl> pImpl;
};

} // namespace modapi::inline addons
