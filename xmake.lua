add_rules("mode.debug", "mode.release")

add_repositories("liteldev-repo https://github.com/LiteLDev/xmake-repo.git")
add_repositories("groupmountain-repo https://github.com/GroupMountain/xmake-repo.git")

add_requires("levilamina 1.5.2", {configs = {target_type = "server"}})
add_requires("levibuildscript 0.5.1")
add_requires("gmlib 1.5.0")
add_requires("minizip-ng 4.0.9")

if not has_config("vs_runtime") then
    set_runtimes("MD")
end

rule("linkrule")
    on_config(function(target)
        target:add("shflags", "/DELAYLOAD:bedrock_runtime.dll")
    end)

    before_link(function(target)
        print("Running prelink...")

        import("lib.detect.find_file")
        import("core.project.config")

        local plat = config.get("plat") or "windows"
        local arch = config.get("arch") or "x64"
        local target_type = config.get("target_type") or "server"

        local builddir = config.builddir()
        local outdir = path.join(builddir, ".prelink")
        local libdir = path.join(outdir, "lib")

        os.mkdir(libdir)

        local data = assert(find_file("bedrock_runtime_data", {"$(env PATH)"}), "Cannot find bedrock_runtime_data")
        local link = assert(find_file("prelink.exe", {"$(env PATH)"}), "Cannot find prelink.exe")

        local inputs = table.copy(target:objectfiles())

        for _, dep in ipairs(target:orderdeps()) do
            print(dep:name())
            if dep:kind() == "static" then
                local libfile = dep:targetfile()
                if libfile and os.isfile(libfile) then
                    table.insert(inputs, libfile)
                end
            end
        end
        for _, pkg in ipairs(target:orderpkgs()) do
            for _, linkdir in ipairs(pkg:get("linkdirs")) do
                for _, link in pairs(pkg:get("links")) do
                    local libfile = path.join(linkdir, link .. ".lib")
                    if string.find(libfile, "static") and os.isfile(libfile) then
                        table.insert(inputs, libfile)
                        print(libfile)
                    end
                end
            end
        end

        os.execv(link, {
            string.format("%s-%s-%s", target_type, plat, arch),
            outdir,
            data,
            table.unpack(inputs)
        })

        target:add("linkdirs", libdir)
        target:add("links", "bedrock_runtime_api")
    end)

    after_link(function(target)
        os.rm("$(builddir)/.prelink/lib/*.lib")
    end)
rule_end()

target("ModApi")
    add_rules("linkrule")
    add_rules("@levibuildscript/modpacker")
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
        "gmlib",
        "minizip-ng"
    )
    set_optimize("aggressive")
    set_exceptions("none")
    set_kind("shared")
    set_languages("c++20")
    set_symbols("debug")
    add_files("src/**.cpp")
    add_includedirs("src", "include")
    add_headerfiles("src/**.h", "include/**.h")