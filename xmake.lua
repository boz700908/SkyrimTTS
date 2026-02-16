-- set minimum xmake version
set_xmakever("2.8.2")

-- includes
includes("lib/commonlibsse-ng")

-- set project
set_project("skyrim-access")
set_version("0.0.1")
set_license("GPL-3.0")

-- set defaults
set_languages("c++23")
set_warnings("allextra")

-- set policies
set_policy("package.requires_lock", true)

-- add rules
add_rules("mode.debug", "mode.releasedbg")
add_rules("plugin.vsxmake.autoupdate")

-- SRAL package (built from local source)
package("sral")
    add_deps("cmake")
    set_sourcedir(path.join(os.scriptdir(), "lib", "SRAL"))
    on_load(function (package)
        if not package:config("shared") then
            package:add("defines", "SRAL_STATIC")
        end
    end)
    on_install(function (package)
        local configs = {}
        table.insert(configs, "-DCMAKE_BUILD_TYPE=" .. (package:debug() and "Debug" or "Release"))
        table.insert(configs, "-DBUILD_SHARED_LIBS=" .. (package:config("shared") and "ON" or "OFF"))
        table.insert(configs, "-DBUILD_TESTING=OFF")
        table.insert(configs, "-DBUILD_EXAMPLES=OFF")
        table.insert(configs, "-DSRAL_DISABLE_UIA=ON")
        table.insert(configs, "-DBUILD_SRAL_TEST=OFF")
        table.insert(configs, "-DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreadedDLL")
        import("package.tools.cmake").install(package, configs, {generator = "NMake Makefiles"})
    end)
package_end()

add_requires("sral", {configs = {shared = false}})
add_requires("nlohmann_json")

-- Mod Organizer 2 installation path (override with XSE_TES5_MODS_PATH env var)
local MO2_MODS_PATH = os.getenv("XSE_TES5_MODS_PATH") or ""
local MOD_FOLDER_NAME = "skyrim-access"

-- targets
target("skyrim-access")
    -- add dependencies to target
    add_deps("commonlibsse-ng")
    add_packages("sral")
    add_packages("nlohmann_json")
    add_defines("SRAL_STATIC")
    add_includedirs("lib/SRAL/Include")

    -- add commonlibsse-ng plugin
    add_rules("commonlibsse-ng.plugin", {
        name = "skyrim-access",
        author = "Amethyst",
        description = "SKSE64 plugin providing accessibility for Skyrim for blind users"
    })

    -- add src files
    add_files("src/**.cpp")
    add_headerfiles("src/**.h")
    add_includedirs("src")
    set_pcxxheader("src/pch.h")
