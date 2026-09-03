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

        local envs = os.joinenvs(target:pkgenvs(), os.getenvs())
        local searchpaths = path.splitenv(envs.PATH or envs.Path or envs.path or "")
        local data = assert(find_file("bedrock_runtime_data", searchpaths), "Cannot find bedrock_runtime_data")
        local link = assert(find_file("prelink.exe", searchpaths), "Cannot find prelink.exe")

        local inputs = {}
        local inputset = {}
        for _, objectfile in ipairs(target:objectfiles()) do
            if not objectfile:endswith(".rc.obj") then
                table.insert(inputs, objectfile)
                inputset[objectfile] = true
            end
        end

        local function add_input(libfile)
            if libfile and os.isfile(libfile) and not inputset[libfile] then
                table.insert(inputs, libfile)
                inputset[libfile] = true
                print(libfile)
            end
        end

        local function add_target_links(dep)
            local linkdirs = table.wrap(dep:get("linkdirs", {public = true}))
            local links = table.wrap(dep:get("links", {public = true}))
            for _, linkdir in ipairs(linkdirs) do
                for _, linkname in ipairs(links) do
                    local libfile = linkname
                    if not path.is_absolute(libfile) then
                        libfile = path.join(linkdir, libfile)
                    end
                    if not libfile:endswith(".lib") then
                        libfile = libfile .. ".lib"
                    end
                    add_input(libfile)
                end
            end
        end

        for _, dep in ipairs(target:orderdeps()) do
            if dep:kind() == "static" then
                add_input(dep:targetfile())
            elseif dep:kind() == "phony" then
                add_target_links(dep)
            end
        end

        for _, pkg in ipairs(target:orderpkgs()) do
            for _, linkdir in ipairs(table.wrap(pkg:get("linkdirs"))) do
                for _, linkname in ipairs(table.wrap(pkg:get("links"))) do
                    local libfile = linkname
                    if not path.is_absolute(libfile) then
                        libfile = path.join(linkdir, libfile)
                    end
                    if not libfile:endswith(".lib") then
                        libfile = libfile .. ".lib"
                    end
                    add_input(libfile)
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
