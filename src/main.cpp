// NorseManifest - the load-order report for the Norse frameworks (UE4SS v4.0.0 C++ mod for NORSE: Oath of Blood),
// 2026-11-17. The checking itself is include/NorseManifest.hpp, which every framework runs for its own files; this mod
// writes the one full picture - every pack, its order or why it is off - to NorseManifest.log at game start.
// Console: nm.list (write the report again, e.g. after adding a pack while the game runs).
#include "NorseManifest.hpp"
#include "Log.hpp"
#include "ue4ss_abi.hpp"

namespace fs = std::filesystem;
using namespace nrn;

namespace
{
    fs::path g_mod;

    fs::path ModDir()
    {
        HMODULE self = nullptr;
        GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                           reinterpret_cast<LPCWSTR>(&ModDir), &self);
        wchar_t buf[MAX_PATH * 2]{};
        GetModuleFileNameW(self, buf, static_cast<DWORD>(std::size(buf)));
        return fs::path(buf).parent_path().parent_path();
    }

    void ReportAll()
    {
        nm::Manifest m;
        m.Resolve(g_mod.parent_path().parent_path(), {}, [](const std::string& s) { NRN_INFO("%s", s.c_str()); });
        int off = 0, warn = 0;
        for (const auto& p : m.Packs())
        {
            off += !p.enabled;
            warn += p.enabled && !p.problems.empty();
        }
        if (off) NRN_WARN("%d pack(s) are switched off - see the OFF lines above", off);
        if (warn) NRN_WARN("%d active pack(s) have a warning", warn);
    }
} // namespace

class NorseManifest : public RC::CppUserModBase
{
  public:
    NorseManifest()
    {
        ModName = L"NorseManifest";
        ModVersion = L"" NM_VERSION;
        ModDescription = L"Pack manifests: requirement checks and one load order for the Norse frameworks' data packs.";
        ModAuthors = L"ApocryphaRealm";
        g_mod = ModDir();
        std::wstring dir;
        int logLevel = 2; // uLogLevel: 0 trace, 1 debug, 2 info, 3 warn, 4 error, 6 off
        {
            std::ifstream in(g_mod / "NorseManifest.ini");
            std::string line;
            while (std::getline(in, line))
            {
                if (!line.empty() && line.back() == '\r') line.pop_back();
                if (line.rfind("sLogDir=", 0) == 0) dir = nm::Widen(line.substr(8));
                if (line.rfind("uLogLevel=", 0) == 0) logLevel = atoi(line.c_str() + 10);
            }
        }
        Log::Get().Open((dir.empty() ? g_mod : fs::path(dir)) / "NorseManifest.log", logLevel);
        Log::Get().Write(Level::Error, "NorseManifest v%s - log level %d", NM_VERSION, logLevel);
        ReportAll();
    }

    auto on_unreal_init() -> void override
    {
        try
        {
            RC::Unreal::Hook::RegisterProcessConsoleExecCallback(
                    [](RC::Unreal::UObject*, const wchar_t* cmd, RC::Unreal::FOutputDevice&, RC::Unreal::UObject*) -> bool {
                        if (!cmd || _wcsicmp(cmd, L"nm.list") != 0) return false;
                        static ULONGLONG last = 0;
                        ULONGLONG now = GetTickCount64();
                        if (now - last < 250) return true;
                        last = now;
                        ReportAll();
                        return true;
                    });
        }
        catch (...)
        {
            NRN_ERROR("console callback registration failed");
        }
    }
};

extern "C" __declspec(dllexport) RC::CppUserModBase* start_mod()
{
    return new NorseManifest();
}

extern "C" __declspec(dllexport) void uninstall_mod(RC::CppUserModBase* mod)
{
    delete mod;
}
