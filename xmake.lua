add_rules("mode.debug", "mode.release")

add_repositories("liteldev-repo https://github.com/LiteLDev/xmake-repo.git")
add_repositories("groupmountain-repo https://github.com/GroupMountain/xmake-repo.git")

add_requires("levilamina 1.7.6", {configs = {target_type = "server"}})
add_requires("levibuildscript 0.5.2")
add_requires("gmlib 1.7.0")

if not has_config("vs_runtime") then
    set_runtimes("MD")
end

target("ModAPI")
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
        "MODAPI_EXPORTS"
    )
    add_packages(
        "levilamina",
        "gmlib"
    )
    set_optimize("aggressive")
    set_exceptions("none")
    set_kind("shared")
    set_languages("c++20")
    set_symbols("debug")
    add_files("src/**.cpp", "src/**.rc")
    add_includedirs("src", "include", "$(builddir)/config")
    add_headerfiles("src/**.h", "include/**.h", "$(builddir)/config/**.h")
    add_configfiles("$(projectdir)/include/(**.*.in)")
    set_configdir("$(builddir)/config")

    on_load(function (target)
        local version_info = import("scripts.get-version-info").get_version_info()
        target:set("configvar", "MODAPI_VERSION_MAJOR", version_info.major)
        target:set("configvar", "MODAPI_VERSION_MINOR", version_info.minor)
        target:set("configvar", "MODAPI_VERSION_PATCH", version_info.patch)
        if version_info.prerelease then
            target:set("configvar", "MODAPI_VERSION_PRERELEASE", version_info.prerelease)
        end
    end)

    after_build(function (target)
        local target_dir = path.join(os.projectdir(), "bin")
        if os.exists(target_dir) then os.rmdir(target_dir) end
        os.cp(target:targetfile(), path.join(target_dir, "dll", "ModAPI", "ModAPI.dll"))
        os.cp(target:symbolfile(), path.join(target_dir, "pdb", "ModAPI.pdb"))
        os.cp(path.join(target:targetdir(), path.basename(target:targetfile()) .. ".lib"), path.join(target_dir, "lib", "ModAPI.lib"))
        import("scripts.generate-manifest", { rootdir = os.projectdir() }).generate_manifest(
            path.join(target_dir, "dll", "ModAPI", "manifest.json"),
            {
                name = "ModAPI",
                entry = "ModAPI.dll",
                version = import("scripts.get-version-info").get_version_info().version_str,
                author = "GroupMountain",
                description = "Group Mountain Mod API",
                passive = is_mode("release")
            }
        )
        os.mkdir(path.join(target_dir, "include"))
        os.cp(path.join(os.projectdir(), "include", "**.h"), path.join(target_dir, "include"), { rootdir = path.join(os.projectdir(), "include") })
        os.cp(path.join(os.projectdir(), "build", "config", "**.h"), path.join(target_dir, "include"), { rootdir = path.join(os.projectdir(), "build", "config") })
    end)