set_xmakever("3.1.1")
set_project("ItemPreviewAutoSpinning")
set_license("GPL-3.0")
set_policy("package.requires_lock", true)

local version = "0.2.0"

add_repositories("bmk https://github.com/gabriel-andreescu/BethesdaModKit.git")
add_addons("bmk 0.3.0")
includes("@addon/bmk/project")
includes("@addon/bmk/native")

-- Dependencies
add_requires("commonlibsse-ng 8.0.1", { system = false })
add_requires("clib-util 1.5.0", { system = false })
add_requires("catch2 3.15.2", { system = false })
add_requires("bmk")

add_requires("caprica", { host = true })
add_requires("skyrim-papyrus-sdk", { configs = { mcm = true } })
option("papyrus_imports", { description = "Papyrus import directories separated by ;" })
option("papyrus_flags", { description = "Optional Papyrus flags file override" })

-- Build targets

target("Native", function()
    set_default(false)
    set_basename("ItemPreviewAutoSpinning")
    set_version(version)
    set_pcxxheader("src/PCH.h")
    add_rules("@commonlibsse-ng/plugin", {
        author = "GabonZ",
        description = "Automatically spins item previews.",
    })
    add_rules("@addon/bmk/skyrim.plugin")
    add_files("$(projectdir)/src/**.cpp")
    add_includedirs("$(projectdir)/src")
    add_defines("WIN32_LEAN_AND_MEAN", "NOGDI")
    add_packages("commonlibsse-ng", "clib-util", "bmk")
end)

target("NativeTests", function()
    set_kind("binary")
    set_default(false)
    add_rules("platform.windows.subsystem")
    set_values("windows.subsystem", "console")
    add_rules("@addon/bmk/native.compiler")
    set_pcxxheader("src/PCH.h")
    add_defines("WIN32_LEAN_AND_MEAN", "NOGDI")
    add_files("src/BoundsMath.cpp", "src/SpinState.cpp", "tests/BoundsMathTests.cpp", "tests/SpinStateTests.cpp")
    add_includedirs("src")
    add_packages("commonlibsse-ng")
    add_packages("catch2", { components = { "main", "lib" } })
    add_tests("native")
end)

target("Mutagen", function()
    set_default(false)
    add_extrafiles("src/mutagen/ItemPreviewAutoSpinning/FormIDs.txt")
    add_rules("@addon/bmk/dotnet", {
        project = "src/mutagen/ItemPreviewAutoSpinning/ItemPreviewAutoSpinning.csproj",
        arguments = { "$(outputdir)", path.absolute("src/mutagen/ItemPreviewAutoSpinning/FormIDs.txt") },
    })
end)

target("MCMScripts", function()
    set_default(false)
    add_rules("@addon/bmk/skyrim.papyrus", {
        root = "src/papyrus/mcm",
        imports = (get_config("papyrus_imports") or ""):split(";", { plain = true }),
        flags = get_config("papyrus_flags"),
        arguments = { "--strict", "--enable-language-extensions=true" },
    })
    add_packages("caprica", "skyrim-papyrus-sdk")
    add_files("$(projectdir)/src/papyrus/mcm/**.psc")
    add_installfiles("$(projectdir)/src/papyrus/mcm/(**.psc)", { prefixdir = "Source/Scripts" })
end)

-- Packages
target("ItemPreviewAutoSpinning", function()
    set_version(version)
    add_rules("@addon/bmk/skyrim.package", {
        targets = {
            "Native",
        },
        nexus = {
            mod_id = "7318624453386",
            file_id = "7467038",
            category = "main",
            primary = true,
            display_name = "Item Preview Auto Spinning",
            description = "Updating from an older version? Follow the Updating to 0.2.0 instructions in the mod description to keep your settings.",
        },
    })
    add_installfiles("$(projectdir)/assets/(**)|optional/**")
end)

target("ItemPreviewAutoSpinningMCM", function()
    set_version(version)
    add_deps("Mutagen", { inherit = false })
    add_rules("@addon/bmk/skyrim.package", {
        targets = { "MCMScripts" },
        package_name = "Item Preview Auto Spinning - MCM Addon",
        nexus = {
            mod_id = "7318624453386",
            file_id = "7997781",
            category = "optional",
            description = "Configure the mod in-game. Requires Item Preview Auto Spinning 0.2.0, SkyUI (or SkyUI VR) and MCM Helper.",
        },
    })
    add_installfiles("$(builddir)/artifacts/Mutagen/mcm/(ItemPreviewAutoSpinning.esp)")
    add_installfiles("$(projectdir)/assets/optional/mcm/(**)")
end)
