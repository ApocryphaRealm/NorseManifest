-- NorseManifest - pack manifests, requirement checks and the load order for the Norse frameworks.
-- include/NorseManifest.hpp is the library (header-only; the frameworks include it). Targets:
--   manifest_test : the offline test (no game) - xmake build manifest_test && xmake run manifest_test
--   main          : the UE4SS mod that writes the full report to NorseManifest.log (needs ../NorsePatcher for UE4SS)
set_project("NorseManifest")
local version = "1.0.0"
set_version(version)
set_xmakever("2.8.0")

add_rules("mode.release")
set_defaultmode("release")
set_languages("c++20")
set_runtimes("MD")
add_defines("WIN32_LEAN_AND_MEAN", "NOMINMAX", "UNICODE", "_UNICODE")
add_cxflags("/EHsc", "/W4", "/permissive-", "/utf-8", {force = true})
add_cxflags({"/d1trimfile:$(projectdir)"}, {force = true, expand = false})

target("manifest_test")
    set_kind("binary")
    add_files("tools/manifest_test.cpp")
    add_includedirs("include")
    set_default(false)

target("main")
    set_kind("shared")
    add_files("src/*.cpp")
    add_includedirs("include", "../NorsePatcher/include")
    add_defines("NM_VERSION=\"" .. version .. "\"")
    -- VERSIONINFO (rule 6): src/version.rc reads these; the numbers come from `version`, which only version-gate writes
    add_files("src/version.rc")
    add_defines("NORSE_VER_STR=\"" .. version .. "\"")
    add_defines("NORSE_VER_MAJOR=" .. version:match("^(%d+)"), "NORSE_VER_MINOR=" .. version:match("^%d+%.(%d+)"),
                "NORSE_VER_PATCH=" .. version:match("^%d+%.%d+%.(%d+)"))
    add_linkdirs("../NorsePatcher/build/ue4ss")
    add_links("UE4SS", "user32")
    set_symbols("debug")
    set_optimize("faster")
    add_shflags("/PDBALTPATH:%_PDB%", {force = true})
    add_shflags("/DEBUG", "/OPT:REF", "/OPT:ICF", {force = true})
