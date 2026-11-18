// NorseManifest.hpp - pack manifests, requirement checks and the load order for the Norse frameworks (2026-11-17).
// Header-only, Windows x64, C++20, no UE4SS dependency (so it also runs in the offline test, tools/manifest_test.cpp).
//
// Every data pack - ours, the community's, and each framework itself - puts one manifest in ue4ss\NorsePacks\*.ini:
//   [Pack]
//   Name     = Tougher Outcasts
//   Version  = 1.2.0
//   Author   = someone
//   Priority = 100                         ; optional; lower loads first (default 0), so higher wins a shared field
//   Requires = NorsePatcher >= 0.1.0, NorseDistributor        ; must be present and enabled (and at least that version)
//   Optional = Bigger Brutes >= 1.0       ; loads before this pack when present; a warning when missing
//   Conflicts = Weaker Outcasts            ; when both are enabled, the one later in the order is switched off
//   GameBuild = EEF010BD-0CB90000          ; optional; only on this game build (the Norse Address Library's BuildId)
//   Files    = Mods/NorsePatcher/patches/tougher-outcasts.ini, Mods/NorseDistributor/distributions/tougher-outcasts.ini
// Files are relative to the ue4ss folder. A framework asks ForFile() about each config file it finds: a file of a
// switched-off pack is skipped; the others are read in pack order, then file name. A file no manifest claims is a loose
// file: it is read after every pack, so a player's own tweak wins.
//
// Modelled on Satisfactory Mod Loader's dependency lists (version ranges, optional dependencies).
#pragma once

#include <Windows.h>

#include <algorithm>
#include <climits>
#include <cstdio>
#include <cwctype>
#include <filesystem>
#include <fstream>
#include <functional>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>

namespace nm
{
namespace fs = std::filesystem;

inline std::wstring Trim(const std::wstring& s)
{
    size_t a = 0, b = s.size();
    while (a < b && iswspace(s[a])) ++a;
    while (b > a && iswspace(s[b - 1])) --b;
    return s.substr(a, b - a);
}

inline std::wstring Lower(std::wstring s)
{
    for (auto& c : s) c = static_cast<wchar_t>(towlower(c));
    return s;
}

inline std::string Narrow(const std::wstring& w)
{
    if (w.empty()) return {};
    int n = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), static_cast<int>(w.size()), nullptr, 0, nullptr, nullptr);
    std::string s(static_cast<size_t>(n), '\0');
    WideCharToMultiByte(CP_UTF8, 0, w.c_str(), static_cast<int>(w.size()), s.data(), n, nullptr, nullptr);
    return s;
}

inline std::wstring Widen(const std::string& s)
{
    if (s.empty()) return {};
    int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), static_cast<int>(s.size()), nullptr, 0);
    std::wstring w(static_cast<size_t>(n), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), static_cast<int>(s.size()), w.data(), n);
    return w;
}

// "1.2.10" -> {1,2,10}; missing parts count as 0.
inline std::vector<int> ParseVersion(const std::wstring& v)
{
    std::vector<int> out;
    std::wstringstream ss(v);
    std::wstring part;
    while (std::getline(ss, part, L'.')) out.push_back(_wtoi(Trim(part).c_str()));
    return out;
}

inline int CompareVersion(const std::vector<int>& a, const std::vector<int>& b)
{
    for (size_t i = 0; i < std::max(a.size(), b.size()); ++i)
    {
        int x = i < a.size() ? a[i] : 0, y = i < b.size() ? b[i] : 0;
        if (x != y) return x < y ? -1 : 1;
    }
    return 0;
}

// The running exe's build id: PE TimeDateStamp-SizeOfImage, as the Norse Address Library names its tables.
inline std::wstring GameBuildId()
{
    auto* base = reinterpret_cast<uint8_t*>(GetModuleHandleW(nullptr));
    auto* dos = reinterpret_cast<IMAGE_DOS_HEADER*>(base);
    auto* nt = reinterpret_cast<IMAGE_NT_HEADERS64*>(base + dos->e_lfanew);
    wchar_t buf[32];
    swprintf(buf, 32, L"%08X-%08X", static_cast<unsigned>(nt->FileHeader.TimeDateStamp), static_cast<unsigned>(nt->OptionalHeader.SizeOfImage));
    return buf;
}

struct Requirement
{
    std::wstring name; // as written
    std::vector<int> minVersion;
    std::wstring minText;
};

struct Pack
{
    std::wstring name, versionText, author, manifest, gameBuild;
    std::vector<int> version;
    int priority = 0;
    std::vector<Requirement> requires_, optional_;
    std::vector<std::wstring> conflicts, files; // files: lower-case, '/' separators, relative to ue4ss
    bool enabled = true;
    std::vector<std::string> problems; // why it is off, or warnings
    int order = -1;
};

struct FileInfo
{
    bool load = true;
    int order = INT_MAX; // pack order; INT_MAX = a loose file (read last)
    std::wstring pack;   // owning pack, empty when loose
    std::string why;     // why it is skipped
};

class Manifest
{
  public:
    using LogFn = std::function<void(const std::string&)>;

    // Reads ue4ssDir\NorsePacks\*.ini and resolves. buildId empty = take it from the running exe.
    void Resolve(const fs::path& ue4ssDir, std::wstring buildId = {}, LogFn log = nullptr)
    {
        m_root = ue4ssDir;
        m_packs.clear();
        m_files.clear();
        if (buildId.empty()) buildId = GameBuildId();
        m_build = buildId;
        std::vector<fs::path> files;
        std::error_code ec;
        fs::path dir = ue4ssDir / "NorsePacks";
        if (fs::is_directory(dir, ec))
            for (const auto& f : fs::directory_iterator(dir, ec))
                if (f.is_regular_file() && Lower(f.path().extension().wstring()) == L".ini") files.push_back(f.path());
        std::sort(files.begin(), files.end());
        for (const auto& f : files) Read(f);
        Check();
        Order();
        for (const Pack& p : m_packs)
            if (p.enabled)
                for (const auto& file : p.files) m_files[file] = &p - &m_packs[0];
        if (log) Report(log);
    }

    FileInfo ForFile(const fs::path& file) const
    {
        FileInfo fi;
        std::wstring rel = Key(file);
        if (auto it = m_owner.find(rel); it != m_owner.end())
        {
            const Pack& p = m_packs[it->second];
            fi.pack = p.name;
            if (!p.enabled)
            {
                fi.load = false;
                fi.why = "its pack '" + Narrow(p.name) + "' is switched off: " + (p.problems.empty() ? std::string("?") : p.problems.front());
                return fi;
            }
            fi.order = p.order;
        }
        return fi;
    }

    // Sorts a framework's config files into load order and drops the ones of switched-off packs (log says why).
    std::vector<fs::path> Arrange(std::vector<fs::path> files, LogFn log = nullptr) const
    {
        std::vector<std::pair<FileInfo, fs::path>> v;
        for (auto& f : files)
        {
            FileInfo fi = ForFile(f);
            if (!fi.load)
            {
                if (log) log("skipped " + Narrow(f.filename().wstring()) + " - " + fi.why);
                continue;
            }
            v.push_back({fi, f});
        }
        std::stable_sort(v.begin(), v.end(), [](const auto& a, const auto& b) {
            if (a.first.order != b.first.order) return a.first.order < b.first.order;
            return Lower(a.second.filename().wstring()) < Lower(b.second.filename().wstring());
        });
        std::vector<fs::path> out;
        for (auto& [fi, f] : v)
        {
            if (log) log((fi.pack.empty() ? std::string("loose file ") : "pack '" + Narrow(fi.pack) + "' ") + Narrow(f.filename().wstring()));
            out.push_back(f);
        }
        return out;
    }

    const std::vector<Pack>& Packs() const { return m_packs; }
    const std::wstring& Build() const { return m_build; }

  private:
    fs::path m_root;
    std::wstring m_build;
    std::vector<Pack> m_packs;
    std::map<std::wstring, size_t> m_files; // enabled packs' files (unused by ForFile; kept for reports)
    std::map<std::wstring, size_t> m_owner; // every claimed file -> its pack, enabled or not

    std::wstring Key(const fs::path& file) const
    {
        std::error_code ec;
        fs::path rel = file.is_absolute() ? fs::relative(file, m_root, ec) : file;
        std::wstring s = Lower(rel.generic_wstring());
        while (!s.empty() && s[0] == L'/') s.erase(0, 1);
        return s;
    }

    static std::vector<Requirement> ParseReqs(const std::wstring& v)
    {
        std::vector<Requirement> out;
        std::wstringstream ss(v);
        std::wstring item;
        while (std::getline(ss, item, L','))
        {
            item = Trim(item);
            if (item.empty()) continue;
            Requirement r;
            size_t op = item.find(L">=");
            if (op != std::wstring::npos)
            {
                r.name = Trim(item.substr(0, op));
                r.minText = Trim(item.substr(op + 2));
                r.minVersion = ParseVersion(r.minText);
            }
            else r.name = item;
            out.push_back(r);
        }
        return out;
    }

    void Read(const fs::path& f)
    {
        std::ifstream in(f, std::ios::binary);
        std::string all((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        if (all.size() >= 3 && static_cast<unsigned char>(all[0]) == 0xEF) all.erase(0, 3);
        Pack p;
        p.manifest = f.filename().wstring();
        std::wstringstream ss(Widen(all));
        std::wstring line;
        while (std::getline(ss, line))
        {
            if (!line.empty() && line.back() == L'\r') line.pop_back();
            size_t c = line.find(L';');
            if (c != std::wstring::npos) line = line.substr(0, c);
            line = Trim(line);
            size_t eq = line.find(L'=');
            if (line.empty() || line[0] == L'[' || eq == std::wstring::npos) continue;
            std::wstring k = Lower(Trim(line.substr(0, eq))), v = Trim(line.substr(eq + 1));
            if (k == L"name") p.name = v;
            else if (k == L"version") p.versionText = v, p.version = ParseVersion(v);
            else if (k == L"author") p.author = v;
            else if (k == L"priority") p.priority = _wtoi(v.c_str());
            else if (k == L"requires") p.requires_ = ParseReqs(v);
            else if (k == L"optional") p.optional_ = ParseReqs(v);
            else if (k == L"conflicts")
                for (auto& r : ParseReqs(v)) p.conflicts.push_back(r.name);
            else if (k == L"gamebuild") p.gameBuild = v;
            else if (k == L"files")
            {
                std::wstringstream fs2(v);
                std::wstring one;
                while (std::getline(fs2, one, L','))
                    if (!Trim(one).empty()) p.files.push_back(Key(fs::path(Trim(one))));
            }
        }
        if (p.name.empty())
        {
            p.name = f.stem().wstring();
            p.enabled = false;
            p.problems.push_back("no Name= line in " + Narrow(p.manifest));
        }
        if (p.versionText.empty()) p.versionText = L"0";
        m_packs.push_back(std::move(p));
    }

    Pack* Find(const std::wstring& name)
    {
        for (auto& p : m_packs)
            if (_wcsicmp(p.name.c_str(), name.c_str()) == 0) return &p;
        return nullptr;
    }

    void Off(Pack& p, const std::string& why)
    {
        if (p.enabled) p.problems.insert(p.problems.begin(), why);
        p.enabled = false;
    }

    void Check()
    {
        // the same name twice: keep the highest version
        for (auto& a : m_packs)
            for (auto& b : m_packs)
                if (&a != &b && a.enabled && b.enabled && _wcsicmp(a.name.c_str(), b.name.c_str()) == 0)
                {
                    Pack& lose = CompareVersion(a.version, b.version) >= 0 ? b : a;
                    Pack& win = &lose == &a ? b : a;
                    Off(lose, "a second copy of '" + Narrow(win.name) + "' (" + Narrow(win.manifest) + ", version " + Narrow(win.versionText) +
                                  ") is used instead");
                }
        for (auto& p : m_packs)
            if (p.enabled && !p.gameBuild.empty() && _wcsicmp(p.gameBuild.c_str(), m_build.c_str()) != 0)
                Off(p, "made for game build " + Narrow(p.gameBuild) + ", this game is " + Narrow(m_build));
        // requirements, repeated until nothing changes (a switched-off pack switches off what needs it)
        for (bool changed = true; changed;)
        {
            changed = false;
            for (auto& p : m_packs)
            {
                if (!p.enabled) continue;
                for (const auto& r : p.requires_)
                {
                    Pack* t = Find(r.name);
                    std::string why;
                    if (!t) why = "needs '" + Narrow(r.name) + "', which is not installed";
                    else if (!t->enabled) why = "needs '" + Narrow(r.name) + "', which is switched off";
                    else if (!r.minVersion.empty() && CompareVersion(t->version, r.minVersion) < 0)
                        why = "needs '" + Narrow(r.name) + "' " + Narrow(r.minText) + " or newer, " + Narrow(t->versionText) + " is installed";
                    if (!why.empty())
                    {
                        Off(p, why);
                        changed = true;
                        break;
                    }
                }
            }
        }
        for (auto& p : m_packs)
            if (p.enabled)
                for (const auto& r : p.optional_)
                {
                    Pack* t = Find(r.name);
                    if (!t || !t->enabled) p.problems.push_back("works better with '" + Narrow(r.name) + "' (not active)");
                    else if (!r.minVersion.empty() && CompareVersion(t->version, r.minVersion) < 0)
                        p.problems.push_back("works better with '" + Narrow(r.name) + "' " + Narrow(r.minText) + " or newer");
                }
        for (size_t i = 0; i < m_packs.size(); ++i)
            for (const auto& f : m_packs[i].files) m_owner[f] = i;
    }

    // Requirements (and present optional packs) first; otherwise by Priority, then name.
    void Order()
    {
        std::vector<Pack*> todo;
        for (auto& p : m_packs)
            if (p.enabled) todo.push_back(&p);
        auto before = [](const Pack* a, const Pack* b) {
            if (a->priority != b->priority) return a->priority < b->priority;
            return Lower(a->name) < Lower(b->name);
        };
        std::set<Pack*> placed;
        int next = 0;
        while (!todo.empty())
        {
            std::vector<Pack*> ready;
            for (Pack* p : todo)
            {
                bool ok = true;
                for (const auto& lst : {&p->requires_, &p->optional_})
                    for (const auto& r : *lst)
                    {
                        Pack* t = Find(r.name);
                        if (t && t->enabled && t != p && !placed.count(t)) ok = false;
                    }
                if (ok) ready.push_back(p);
            }
            if (ready.empty())
            {
                for (Pack* p : todo) Off(*p, "its requirements form a loop");
                break;
            }
            Pack* first = *std::min_element(ready.begin(), ready.end(), before);
            first->order = next++;
            placed.insert(first);
            todo.erase(std::find(todo.begin(), todo.end(), first));
        }
        // conflicts: the later of two enabled conflicting packs goes off
        for (auto& p : m_packs)
            for (const auto& c : p.conflicts)
                if (Pack* t = Find(c); t && t->enabled && p.enabled)
                {
                    Pack& late = p.order > t->order ? p : *t;
                    Pack& early = &late == &p ? *t : p;
                    Off(late, "conflicts with '" + Narrow(early.name) + "', which loads first");
                }
        // number the enabled packs 0..n-1 again (a conflict may have switched off a placed pack)
        std::vector<Pack*> on;
        for (auto& p : m_packs)
            if (p.enabled) on.push_back(&p);
        std::sort(on.begin(), on.end(), [](const Pack* a, const Pack* b) { return a->order < b->order; });
        for (size_t i = 0; i < on.size(); ++i) on[i]->order = static_cast<int>(i);
        for (auto& p : m_packs)
            if (!p.enabled) p.order = -1;
    }

    void Report(const LogFn& log) const
    {
        std::vector<const Pack*> v;
        for (const auto& p : m_packs) v.push_back(&p);
        std::sort(v.begin(), v.end(), [](const Pack* a, const Pack* b) {
            int x = a->enabled ? a->order : INT_MAX, y = b->enabled ? b->order : INT_MAX;
            return x != y ? x < y : Lower(a->name) < Lower(b->name);
        });
        int on = 0;
        for (const Pack* p : v) on += p->enabled;
        log("packs: " + std::to_string(v.size()) + " manifest(s), " + std::to_string(on) + " active, game build " + Narrow(m_build));
        for (const Pack* p : v)
        {
            char head[64];
            if (p->enabled) snprintf(head, sizeof(head), "  %2d. ", p->order + 1);
            else snprintf(head, sizeof(head), "  OFF ");
            std::string line = head + Narrow(p->name) + " " + Narrow(p->versionText) + " (" + Narrow(p->manifest) + ", " +
                               std::to_string(p->files.size()) + " file(s))";
            if (!p->problems.empty()) line += " - " + p->problems.front();
            log(line);
            for (size_t i = 1; i < p->problems.size(); ++i) log("       also: " + p->problems[i]);
        }
    }
};
} // namespace nm
