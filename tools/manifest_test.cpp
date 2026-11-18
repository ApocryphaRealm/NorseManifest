// manifest_test.cpp - offline test of NorseManifest.hpp (no game, no UE4SS): writes sample manifests to a temp folder,
// resolves them for a fake game build, and checks every rule. Exit code 0 = all passed.
#include "NorseManifest.hpp"

#include <cstdio>

namespace fs = std::filesystem;

static int g_fail = 0, g_pass = 0;
#define CHECK(cond, what)                                                                                                    \
    do                                                                                                                       \
    {                                                                                                                        \
        if (cond) ++g_pass;                                                                                                  \
        else                                                                                                                 \
        {                                                                                                                    \
            ++g_fail;                                                                                                        \
            std::printf("FAIL: %s\n", what);                                                                                 \
        }                                                                                                                    \
    } while (0)

static void Write(const fs::path& p, const char* text)
{
    fs::create_directories(p.parent_path());
    std::ofstream(p, std::ios::binary) << text;
}

static const nm::Pack* P(const nm::Manifest& m, const wchar_t* name)
{
    for (const auto& p : m.Packs())
        if (p.name == name && p.enabled) return &p;
    for (const auto& p : m.Packs())
        if (p.name == name) return &p;
    return nullptr;
}

int main()
{
    fs::path root = fs::temp_directory_path() / "norse-manifest-test";
    std::error_code ec;
    fs::remove_all(root, ec);
    fs::path packs = root / "NorsePacks";
    Write(packs / "_NorsePatcher.ini", "[Pack]\nName=NorsePatcher\nVersion=0.1.0\n");
    Write(packs / "_NorseDistributor.ini", "[Pack]\nName=NorseDistributor\nVersion=0.1.0\n");
    Write(packs / "tougher.ini",
          "[Pack]\nName=Tougher Outcasts\nVersion=1.2.0\nPriority=10\nRequires=NorsePatcher >= 0.1.0\n"
          "Files=Mods/NorsePatcher/patches/tough.ini\n");
    Write(packs / "missing.ini", "[Pack]\nName=Needs Missing\nVersion=1\nRequires=NorseTagger\n");
    Write(packs / "newer.ini", "[Pack]\nName=Needs New Patcher\nVersion=1\nRequires=NorsePatcher >= 0.2.0\n");
    Write(packs / "chain.ini", "[Pack]\nName=Chain\nVersion=1\nRequires=Needs Missing\n");
    Write(packs / "build.ini", "[Pack]\nName=Wrong Build\nVersion=1\nGameBuild=DEADBEEF-00000000\n");
    Write(packs / "rightbuild.ini", "[Pack]\nName=Right Build\nVersion=1\nGameBuild=EEF010BD-0CB90000\n");
    Write(packs / "weaker.ini",
          "[Pack]\nName=Weaker Outcasts\nVersion=1\nPriority=20\nConflicts=Tougher Outcasts\n"
          "Files=Mods/NorsePatcher/patches/weak.ini\n");
    Write(packs / "base.ini", "[Pack]\nName=Base Pack\nVersion=2.0\nPriority=50\nFiles=Mods/NorsePatcher/patches/base.ini\n");
    Write(packs / "addon.ini",
          "[Pack]\nName=Addon\nVersion=1\nPriority=0\nRequires=Base Pack >= 2\nOptional=Not Here, NorseDistributor\n"
          "Files=Mods/NorsePatcher/patches/addon.ini\n");
    Write(packs / "dup1.ini", "[Pack]\nName=Dup\nVersion=1.0\n");
    Write(packs / "dup2.ini", "[Pack]\nName=Dup\nVersion=1.1\n");
    Write(packs / "loop1.ini", "[Pack]\nName=Loop A\nVersion=1\nRequires=Loop B\n");
    Write(packs / "loop2.ini", "[Pack]\nName=Loop B\nVersion=1\nRequires=Loop A\n");
    Write(packs / "noname.ini", "[Pack]\nVersion=1\n");

    nm::Manifest m;
    m.Resolve(root, L"EEF010BD-0CB90000", [](const std::string& s) { std::printf("%s\n", s.c_str()); });

    auto on = [&](const wchar_t* n) { auto* p = P(m, n); return p && p->enabled; };
    CHECK(on(L"NorsePatcher") && on(L"NorseDistributor"), "frameworks are packs and load");
    CHECK(on(L"Tougher Outcasts"), "a met requirement with a version loads");
    CHECK(!on(L"Needs Missing"), "a missing requirement switches the pack off");
    CHECK(!on(L"Needs New Patcher"), "a too-old requirement switches the pack off");
    CHECK(!on(L"Chain"), "a pack needing a switched-off pack goes off too");
    CHECK(!on(L"Wrong Build") && on(L"Right Build"), "GameBuild is honoured");
    CHECK(!on(L"Weaker Outcasts"), "of two conflicting packs the later one goes off");
    CHECK(on(L"Addon") && on(L"Base Pack") && P(m, L"Base Pack")->order < P(m, L"Addon")->order,
          "a requirement loads before its dependent, whatever the priorities");
    CHECK(P(m, L"Addon")->problems.size() == 1, "a missing optional pack is a warning, a present one is not");
    CHECK(P(m, L"NorseDistributor")->order < P(m, L"Addon")->order, "a present optional pack loads first");
    int dupOn = 0;
    for (const auto& p : m.Packs())
        if (p.name == L"Dup" && p.enabled)
        {
            ++dupOn;
            CHECK(p.versionText == L"1.1", "the highest copy of a duplicate wins");
        }
    CHECK(dupOn == 1, "only one copy of a duplicate stays on");
    CHECK(!on(L"Loop A") && !on(L"Loop B"), "a requirement loop switches its packs off");
    CHECK(!on(L"noname"), "a manifest without Name is off");
    CHECK(P(m, L"Tougher Outcasts")->order < P(m, L"Weaker Outcasts")->order || !on(L"Weaker Outcasts"), "priority order");

    fs::path pdir = root / "Mods" / "NorsePatcher" / "patches";
    std::vector<fs::path> files = {pdir / "loose.ini", pdir / "weak.ini", pdir / "addon.ini", pdir / "tough.ini", pdir / "base.ini"};
    auto arranged = m.Arrange(files, [](const std::string& s) { std::printf("  arrange: %s\n", s.c_str()); });
    std::vector<std::wstring> names;
    for (auto& f : arranged) names.push_back(f.filename().wstring());
    CHECK(std::find(names.begin(), names.end(), L"weak.ini") == names.end(), "a switched-off pack's file is skipped");
    CHECK(!names.empty() && names.back() == L"loose.ini", "a loose file loads last");
    auto idx = [&](const wchar_t* n) { return std::find(names.begin(), names.end(), n) - names.begin(); };
    CHECK(idx(L"tough.ini") < idx(L"base.ini") && idx(L"base.ini") < idx(L"addon.ini"), "files follow the pack order");

    std::printf("\n%d passed, %d failed\n", g_pass, g_fail);
    fs::remove_all(root, ec);
    return g_fail ? 1 : 0;
}
