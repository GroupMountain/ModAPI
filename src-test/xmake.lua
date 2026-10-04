-- Test mod: a regular LeviLamina native mod that links against ModAPI and exercises it on a live
-- server. `set_default(false)` keeps it out of the normal build; build it explicitly:
--
--     xmake build test
--
-- The output lands in bin/test/ and can be copied into the server's mods directory next to
-- bin/dll/ModAPI.
target("test")
    set_default(false)
    set_kind("shared")
    add_deps("ModAPI")
    set_exceptions("cxx")
    add_rules("@levibuildscript/linkrule")
    add_cxflags(
        "/EHsc",
        "/utf-8",
        "/W4",
        "/w44265",
        "/w44289",
        "/w44296",
        "/w45263",
        "/w44738",
        "/w45204",
        "/we4297",
        "/O2",
        "/Ob3",
        "/GR-",
        "/Zo-"
    )
    add_defines(
        "NOMINMAX",
        "UNICODE",
        "_HAS_CXX23=1",
        "LL_PLAT_S"
    )
    add_packages("levilamina")
    add_includedirs("$(projectdir)/include", "$(projectdir)/src", "$(builddir)/config")
    set_optimize("aggressive")
    set_languages("c++20")
    set_symbols("debug")
    add_files("$(projectdir)/src-test/**.cpp")
    -- Local definitions for the BDS symbols the prelink cannot resolve (see the file's comment); the
    -- test constructs `ItemInstance`s too, so it needs them as well.
    add_files("$(projectdir)/src/mc/ItemInstance.cpp", "$(projectdir)/src/mc/StaticOptimizedString.cpp", "$(projectdir)/src/modapi/semversion/SemVersion.cpp")
    after_build(function (target)
        local target_dir = path.join(os.projectdir(), "bin", "test")
        if os.exists(target_dir) then os.rmdir(target_dir) end
        os.cp(target:targetfile(), path.join(target_dir, "dll", "test", "test.dll"))
        os.cp(target:symbolfile(), path.join(target_dir, "pdb", "test.pdb"))
        import("scripts.generate-manifest", { rootdir = os.projectdir() }).generate_manifest(
            path.join(target_dir, "dll", "test", "manifest.json"),
            {
                name = "test",
                entry = "test.dll",
                version = import("scripts.get-version-info", { rootdir = os.projectdir() }).get_version_info().version_str,
                author = "GroupMountain",
                description = "ModAPI test mod",
                passive = false,
                dependencies = {
                    {
                        name = "ModAPI"
                    }
                }
            }
        )
    end)
