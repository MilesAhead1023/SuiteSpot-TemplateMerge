#include <filesystem>
#include <cstdlib>            // for std::getenv and system
#include <system_error>       // for std::error_code
#include "SuiteSpotConfig.h" // configuration helpers


namespace ss_paths {
    namespace fs = std::filesystem;

    inline fs::path epicModsDefault() {
        const char* up = std::getenv("USERPROFILE");
        if (!up) return {};
        return fs::path(up) / "Documents" / "My Games" / "Rocket League" / "TAGame" / "CookedPCConsole" / "mods";
    }

    inline fs::path steamWorkshopDefault() {
        const char* sp = std::getenv("PROGRAMFILES(X86)");
        if (!sp) return {};
        fs::path base = fs::path(sp) / "Steam" / "steamapps" / "workshop" / "content" / "252950";
        if (fs::exists(base)) return base;
        return {};
    }

    inline fs::path bmDataRoot() {
        const char* app = std::getenv("APPDATA");
        if (!app) return {};
        return fs::path(app) / "bakkesmod" / "bakkesmod" / "data";
    }

    inline fs::path suiteTrainingDir()  { return bmDataRoot() / "SuiteTraining"; }
    inline fs::path suiteWorkshopsDir() { return bmDataRoot() / "SuiteWorkshops"; }

    inline void ensureDataDirs(std::error_code& ec) {
        fs::create_directories(suiteTrainingDir(),  ec);
        ec.clear();
        fs::create_directories(suiteWorkshopsDir(), ec);
    }
}

// Helper functions for handling Epic/Steam workshop maps and cooked content.
// These functions are designed to avoid reliance on WorkshopMapLoader and do
// not probe the user's Documents folder. Instead, they prioritise the
// BakkesMod data directories and standard Steam workshop paths.
namespace ss_epic {
    namespace fs = std::filesystem;

    // Returns true if the given path exists and is a directory.
    inline bool exists_dir(const fs::path& p) {
        std::error_code ec;
        return fs::exists(p, ec) && fs::is_directory(p, ec);
    }

    // Determines whether a file path has an extension that looks like a
    // workshop map (upk, udk, pak, zip). Extensions are compared case-
    // insensitively.
    inline bool looksLikeMapFile(const fs::path& p) {
        auto ext = p.extension().string();
        for (auto& c : ext) c = (char)tolower((unsigned char)c);
        return ext == ".udk" || ext == ".upk" || ext == ".pak" || ext == ".zip";
    }

    // Returns true if the directory contains at least one map file directly or
    // within its immediate children (one level deep). This avoids scanning
    // deeply nested directories but is sufficient for our auto-detection.
    inline bool looksLikeMapDir(const fs::path& p) {
        std::error_code ec;
        if (!exists_dir(p)) return false;
        // Check files in current directory
        for (auto& e : fs::directory_iterator(p, ec)) {
            if (ec) { ec.clear(); continue; }
            if (e.is_regular_file(ec) && looksLikeMapFile(e.path())) return true;
        }
        // Check immediate subdirectories
        for (auto& e : fs::directory_iterator(p, ec)) {
            if (ec) { ec.clear(); continue; }
            if (!e.is_directory(ec)) continue;
            for (auto& f : fs::directory_iterator(e.path(), ec)) {
                if (ec) { ec.clear(); continue; }
                if (f.is_regular_file(ec) && looksLikeMapFile(f.path())) return true;
            }
        }
        return false;
    }

    // Build a list of candidate directories where workshop maps may reside.
    // The order is: SuiteSpot's own data folders, generic 'Workshop' folder
    // within BakkesMod data, and Steam workshop content for Rocket League.
    inline std::vector<fs::path> candidateFolders() {
        std::vector<fs::path> cand;
        const char* app = std::getenv("APPDATA");
        if (app) {
            fs::path dataRoot = fs::path(app) / "bakkesmod" / "bakkesmod" / "data";
            cand.push_back(dataRoot / "SuiteWorkshops");
            cand.push_back(dataRoot / "Workshop");
        }
        const char* pf86 = std::getenv("PROGRAMFILES(X86)");
        if (pf86) {
            fs::path base = fs::path(pf86) / "Steam" / "steamapps" / "workshop" / "content" / "252950";
            cand.push_back(base);
            // Attempt to parse additional Steam library folders from libraryfolders.vdf
            fs::path vdf = fs::path(pf86) / "Steam" / "steamapps" / "libraryfolders.vdf";
            std::error_code ec;
            if (fs::exists(vdf, ec) && fs::is_regular_file(vdf, ec)) {
                std::ifstream in(vdf.string());
                std::string line;
                while (std::getline(in, line)) {
                    auto pos = line.find("\"path\"");
                    if (pos == std::string::npos) continue;
                    auto q1 = line.find('"', pos + 6);
                    if (q1 == std::string::npos) continue;
                    auto q2 = line.find('"', q1 + 1);
                    if (q2 == std::string::npos) continue;
                    std::string p = line.substr(q1 + 1, q2 - (q1 + 1));
                    fs::path lib = fs::path(p) / "steamapps" / "workshop" / "content" / "252950";
                    cand.push_back(lib);
                }
            }
        }
        return cand;
    }

    // Attempt to detect a workshop root directory by iterating candidate
    // directories. If a directory contains recognizable map files, it is
    // returned. If none match, an empty path is returned.
    inline fs::path detectWorkshopRoot() {
        for (const auto& c : candidateFolders()) {
            std::error_code ec;
            if (looksLikeMapDir(c)) return c;
        }
        return {};
    }

    // Check if essential cooked texture files exist in the cooked folder. This
    // matches the file list typically used by WorkshopMapLoader to determine
    // whether textures are installed. The list is intentionally conservative.
    inline bool texturesInstalled(const fs::path& cooked) {
        static const char* required[] = {
            "mods.upk",
            "Engine_MI_Shaders.upk",
            "EngineBuildings.upk",
            "EngineDebugMaterials.upk",
            "MapTemplates.upk",
            "MapTemplateIndex.upk",
            "NodeBuddies.upk"
        };
        for (auto* file : required) {
            if (!exists_dir(cooked)) return false;
            std::error_code ec;
            if (!fs::exists(cooked / file, ec)) return false;
        }
        return true;
    }

    // Expand a zip archive into the destination directory using PowerShell. The
    // -Force flag overwrites existing files. This helper spawns a shell via
    // system() and therefore blocks until completion. It assumes
    // powershell.exe is available on the system.
    inline void ps_expand_zip(const std::string& zipPath, const fs::path& dest) {
        // Ensure destination directory exists
        std::error_code ec;
        fs::create_directories(dest, ec);
        // Build and execute the PowerShell command
        std::string cmd = std::string("powershell.exe -NoProfile -ExecutionPolicy Bypass -Command \"") +
                          "Expand-Archive -LiteralPath '" + zipPath + "' -DestinationPath '" + dest.string() + "' -Force\"";
        std::system(cmd.c_str());
    }

    // Download a file from a URL using PowerShell Invoke-WebRequest and save
    // it to the specified output file. Returns true on success. If PowerShell
    // fails for any reason, false is returned. Note: this function relies on
    // an outbound connection and may fail silently if network access is not
    // available.
    inline bool ps_download_to(const std::string& url, const std::string& outFile) {
        std::string cmd = std::string("powershell.exe -NoProfile -ExecutionPolicy Bypass -Command \"") +
                          "Invoke-WebRequest -Uri '" + url + "' -OutFile '" + outFile + "' -UseBasicParsing\"";
        int code = std::system(cmd.c_str());
        return code == 0;
    }
} // namespace ss_epic


#include "pch.h"
#include "SuiteSpot.h"
#include "MapList.h"
#include <fstream>
#include <string>
#include <algorithm>
#include <random>
#include <utility>

// ===== SuiteSpot persistence helpers =====
std::filesystem::path SuiteSpot::GetDataRoot() const {
    const char* appdata = std::getenv("APPDATA");
    if (!appdata) return std::filesystem::path();
    return std::filesystem::path(appdata) / "bakkesmod" / "bakkesmod" / "data";
}
std::filesystem::path SuiteSpot::GetSuiteTrainingDir() const { return GetDataRoot() / "SuiteTraining"; }
std::filesystem::path SuiteSpot::GetSuiteWorkshopsDir() const { return GetDataRoot() / "SuiteWorkshops"; }
std::filesystem::path SuiteSpot::GetTrainingFilePath() const { return GetSuiteTrainingDir() / "SuiteSpotTrainingMaps.txt"; }
std::filesystem::path SuiteSpot::GetWorkshopFilePath() const { return GetSuiteWorkshopsDir() / "(mirror-only/no-manifest)"; }

void SuiteSpot::EnsureDataDirectories() const const {
    std::error_code ec;
    auto root = GetDataRoot();
    if (!root.empty()) std::filesystem::create_directories(root, ec);
    ec.clear(); std::filesystem::create_directories(GetSuiteTrainingDir(), ec);
    ec.clear(); std::filesystem::create_directories(GetSuiteWorkshopsDir(), ec);
}

void SuiteSpot::LoadTrainingMaps() {
    EnsureDataDirectories();
    EnsureReadmeFiles();
    RLTraining.clear();
    auto f = GetTrainingFilePath();
    std::error_code ec;
    if (!std::filesystem::exists(f, ec)) return;
    std::ifstream in(f.string());
    if (!in.is_open()) return;
    std::string line;
    while (std::getline(in, line)) {
        if (line.empty()) continue;
        auto pos = line.find(',');
        if (pos == std::string::npos) continue;
        std::string code = line.substr(0, pos);
        std::string name = line.substr(pos+1);
        if (!code.empty() && !name.empty()) RLTraining.push_back({ code, name });
    }
}

void SuiteSpot::SaveTrainingMaps() const {
    auto f = GetTrainingFilePath();
    EnsureDataDirectories();
    EnsureReadmeFiles();
    std::ofstream out(f.string(), std::ios::trunc);
    if (!out.is_open()) return;
    for (const auto& e : RLTraining) out << e.code << "," << e.name << "\n";
}







// Mirror src directory recursively into dst
void SuiteSpot::MirrorDirectory(const std::filesystem::path& src, const std::filesystem::path& dst) const {
    std::error_code ec;
    if (!std::filesystem::exists(src, ec) || !std::filesystem::is_directory(src, ec)) return;
    std::filesystem::create_directories(dst, ec);
    for (auto const& entry : std::filesystem::recursive_directory_iterator(src, ec)) {
        if (ec) { ec.clear(); continue; }
        auto rel = std::filesystem::relative(entry.path(), src, ec);
        if (ec) { ec.clear(); continue; }
        auto target = dst / rel;
        if (entry.is_directory()) {
            std::filesystem::create_directories(target, ec);
        } else if (entry.is_regular_file()) {
            // copy if missing or different timestamp/size
            bool copyNeeded = true;
            if (std::filesystem::exists(target, ec)) {
                auto src_time = std::filesystem::last_write_time(entry.path(), ec);
                auto dst_time = std::filesystem::last_write_time(target, ec);
                auto src_size = std::filesystem::file_size(entry.path(), ec);
                auto dst_size = std::filesystem::file_size(target, ec);
                copyNeeded = (src_time != dst_time) || (src_size != dst_size);
            }
            if (copyNeeded) {
                std::filesystem::create_directories(target.parent_path(), ec);
                std::filesystem::copy_file(entry.path(), target, std::filesystem::copy_options::overwrite_existing, ec);
            }
        }
    }
}

void SuiteSpot::EnsureReadmeFiles() const {
    // SuiteTraining README
    auto tr = GetSuiteTrainingDir() / "README.txt";
    if (!std::filesystem::exists(tr)) {
        std::ofstream o(tr.string(), std::ios::trunc);
        o << "SuiteTraining\\SuiteSpotTrainingMaps.txt\n"
             "CSV format:\n"
             "    <training_code>,<display_name>\n"
             "One entry per line. This file is read on game start and updated when you add a map in SuiteSpot.\n";
    }
    // SuiteWorkshops README
    auto wr = GetSuiteWorkshopsDir() / "README.txt";
    if (!std::filesystem::exists(wr)) {
        std::ofstream o(wr.string(), std::ios::trunc);
        o << "SuiteWorkshops is a mirrored copy of your Rocket League 'mods' folder.\n"
             "Origin (Epic): C:\\Program Files\\Epic Games\\rocketleague\\TAGame\\CookedPCConsole\\mods\n"
             "On game start, SuiteSpot mirrors that folder here for persistence and indexing.\n"
             "Do not edit map files here unless you know what you're doing.\n";
    }
}


void SuiteSpot::DiscoverWorkshopInDir(const std::filesystem::path& dir) {
    std::error_code ec;
    if (!std::filesystem::exists(dir, ec) || !std::filesystem::is_directory(dir, ec)) return;
    for (const auto& entry : std::filesystem::directory_iterator(dir, ec)) {
        if (ec) { ec.clear(); continue; }
        if (entry.is_directory()) {
            bool found = false;
            for (const auto& f : std::filesystem::directory_iterator(entry.path(), ec)) {
                if (ec) { ec.clear(); continue; }
                if (f.is_regular_file() && f.path().extension() == ".upk") {
                    RLWorkshop.push_back({ f.path().string(), entry.path().filename().string() });
                    found = true;
                    break;
                }
            }
        } else if (entry.is_regular_file() && entry.path().extension() == ".upk") {
            RLWorkshop.push_back({ entry.path().string(), entry.path().stem().string() });
        }
    }
}


void SuiteSpot::SaveWorkshopMaps() const {
    // No-op: SuiteWorkshops uses mirrored mods; no manifest file.
}


using namespace std;
using namespace std::filesystem;


void SuiteSpot::LoadWorkshopMaps()
{
    RLWorkshop.clear();

    namespace fs = std::filesystem;
    std::error_code ec;

    // Roots for Epic + Steam (we scan recursively)
    const std::vector<fs::path> roots = {
        fs::path{R"(C:\Program Files\Epic Games\rocketleague\TAGame\CookedPCConsole\mods)"},
        fs::path{R"(C:\Program Files (x86)\Steam\steamapps\workshop\content\252950)"}
    };

    auto readJsonTitle = [](const fs::path& jsonPath) -> std::string {
        std::ifstream in(jsonPath);
        if (!in.is_open()) return {};
        std::string s((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        auto t = s.find("\"title\"");
        if (t == std::string::npos) return {};
        t = s.find(':', t); if (t == std::string::npos) return {};
        t = s.find('"', t); if (t == std::string::npos) return {};
        auto e = s.find('"', t + 1); if (e == std::string::npos) return {};
        return s.substr(t + 1, e - (t + 1));
    };

    for (const auto& root : roots)
    {
        if (!fs::exists(root, ec) || ec) { ec.clear(); continue; }

        for (fs::recursive_directory_iterator it(root, ec), end; it != end; ++it) {
            if (ec) { ec.clear(); continue; }
            if (!it->is_regular_file(ec)) { if (ec) ec.clear(); continue; }

            const fs::path p = it->path();
            if (p.extension() != ".upk") continue;

            WorkshopEntry w;
            w.filePath = p.string(); // full path saved
            std::string stem = p.stem().string();

            // Pretty name: json title > parent folder > stem
            std::string display = stem;
            fs::path parent = p.parent_path().filename();
            if (!parent.empty()) display = parent.string();
            fs::path jsonPath = p.parent_path() / (stem + ".json");
            if (fs::exists(jsonPath, ec) && !ec) {
                auto t = readJsonTitle(jsonPath);
                if (!t.empty()) display = t;
            }
            w.name = display;
            RLWorkshop.push_back(std::move(w));
        }
    }

    std::sort(RLWorkshop.begin(), RLWorkshop.end(),
              [](const WorkshopEntry& a, const WorkshopEntry& b){ return a.name < b.name; });

    currentWorkshopIndex = std::clamp(currentWorkshopIndex, 0, (int)RLWorkshop.size() - 1);
}

    // Nice to have: sort alphabetically
    std::sort(RLWorkshop.begin(), RLWorkshop.end(),
              [](const WorkshopEntry& a, const WorkshopEntry& b){ return a.name < b.name; });
}


using namespace std;
using namespace std::chrono_literals;

BAKKESMOD_PLUGIN(SuiteSpot, "SuiteSpot", plugin_version, PLUGINTYPE_FREEPLAY)

shared_ptr<CVarManagerWrapper> _globalCvarManager;

void SuiteSpot::SaveSettings() {
    std::ofstream file("suitespot_settings.cfg");
    if (!file.is_open()) return;
    file << (autoQueue ? 1 : 0) << "\n";
    file << mapType << "\n";
    file << delayQueueSec << "\n";
    file << delayFreeplaySec << "\n";
    file << delayTrainingSec << "\n";
    file << delayWorkshopSec << "\n";
    file << currentIndex << "\n";
    file << currentTrainingIndex << "\n";
    file << currentWorkshopIndex << "\n";
    file.close();
}

void SuiteSpot::LoadSettings() {
    std::ifstream file("suitespot_settings.cfg");
    if (!file.is_open()) return;
    int aq=0, mt=0, dq=0, df=0, dt=0, dw=0, ci=0, cti=0, cwi=0;
    file >> aq >> mt >> dq >> df >> dt >> dw >> ci >> cti >> cwi;
    autoQueue = aq!=0;
    mapType = mt;
    delayQueueSec = dq;
    delayFreeplaySec = df;
    delayTrainingSec = dt;
    delayWorkshopSec = dw;
    currentIndex = ci;
    currentTrainingIndex = cti;
    currentWorkshopIndex = cwi;
}

void SuiteSpot::LoadHooks() {
    // Re-queue/transition at match end or when main menu appears after a match
    gameWrapper->HookEvent("Function TAGame.GameEvent_Soccar_TA.EventMatchEnded", bind(&SuiteSpot::GameEndedEvent, this, placeholders::_1));
    gameWrapper->HookEvent("Function TAGame.AchievementManager_TA.HandleMatchEnded", bind(&SuiteSpot::GameEndedEvent, this, placeholders::_1));
}

void SuiteSpot::GameEndedEvent(std::string name) {
    if (!enabled) return;

    auto safeExecute = [&](int delaySec, const std::string& cmd) {
        if (delaySec <= 0) {
            cvarManager->executeCommand(cmd);
        } else {
            gameWrapper->SetTimeout([this, cmd](GameWrapper* gw) { cvarManager->executeCommand(cmd); }, static_cast<float>(delaySec));
        }
    };

    // Dispatch based on mapType
    if (mapType == 0) { // Freeplay
        if (currentIndex < 0 || currentIndex >= (int)RLMaps.size()) {
            LOG("SuiteSpot: Freeplay index out of range; skipping load.");
        } else {
            safeExecute(delayFreeplaySec, "load_freeplay " + RLMaps[currentIndex].code);
            LOG("SuiteSpot: Loading freeplay map: " + RLMaps[currentIndex].name);
        }
    } else if (mapType == 1) { // Training
        if (RLTraining.empty()) {
            LOG("SuiteSpot: No training maps configured.");
        } else {
            // Clamp or choose from shuffle
            if (trainingShuffleEnabled) {
                currentTrainingIndex = NextTrainingIndex();
            } else {
                currentTrainingIndex = std::clamp(currentTrainingIndex, 0, (int)RLTraining.size()-1);
            }
            safeExecute(delayTrainingSec, "load_training " + RLTraining[currentTrainingIndex].code);
            LOG("SuiteSpot: Loading training map: " + RLTraining[currentTrainingIndex].name);
        }
    } else if (mapType == 2) { // Workshop
        if (RLWorkshop.empty()) {
            LOG("SuiteSpot: No workshop maps configured.");
        } else {
            currentWorkshopIndex = std::clamp(currentWorkshopIndex, 0, (int)RLWorkshop.size()-1);
            safeExecute(delayWorkshopSec, "load_workshop \"" + RLWorkshop[currentWorkshopIndex].filePath + "\"");
            LOG("SuiteSpot: Loading workshop map: " + RLWorkshop[currentWorkshopIndex].name);
        }
    }

    if (autoQueue) {
        safeExecute(delayQueueSec, "queue");
        LOG("SuiteSpot: Auto-Queuing triggered.");
    }
}

void SuiteSpot::onLoad() {
// Register SuiteSpot CVars and notifiers
    cvarManager->registerCvar("suitespot_autoqueue", "0", "Enable auto-queue", true, true, 0, true, 1);
    cvarManager->registerCvar("suitespot_delay_freeplay", "2", "Delay before freeplay (seconds)");
    cvarManager->registerCvar("suitespot_delay_training", "2", "Delay before training (seconds)");
    cvarManager->registerCvar("suitespot_delay_workshop", "2", "Delay before workshop (seconds)");
    // Workshop folder path used when selecting maps manually. If left blank,
    // detection will attempt to locate a suitable directory on demand.
    cvarManager->registerCvar("suitespot_workshop_path", "", "Workshop folder path")
        .addOnValueChanged([this](std::string, CVarWrapper c) {
            // Persist workshop path to configuration whenever it changes
            ss_cfg::write("workshop_path", c.getStringValue());
        });

    // Path to the Epic cooked content directory (TAGame\CookedPCConsole). Users
    // should set this if they wish to download workshop textures or import maps
    // for Epic versions of Rocket League.
    cvarManager->registerCvar("suitespot_cooked_path", "", "Epic CookedPCConsole path (TAGame\\CookedPCConsole)")
        .addOnValueChanged([this](std::string, CVarWrapper c) {
            ss_cfg::write("cooked_path", c.getStringValue());
        });

    // Folder containing workshop maps to import into the cooked folder. When
    // populated, clicking the import button will copy or unpack maps from
    // this folder into the cooked directory.
    cvarManager->registerCvar("suitespot_import_from", "", "Folder containing workshop maps to import")
        .addOnValueChanged([this](std::string, CVarWrapper c) {
            ss_cfg::write("import_from", c.getStringValue());
        });

    // Read persisted configuration values and populate CVars accordingly. This
    // ensures that user preferences survive between game sessions. If a key
    // is missing from the config file, the corresponding CVar retains its
    // default empty value.
    {
        std::string v;
        v = ss_cfg::read("workshop_path");
        if (!v.empty()) cvarManager->getCvar("suitespot_workshop_path").setValue(v);
        v = ss_cfg::read("cooked_path");
        if (!v.empty()) cvarManager->getCvar("suitespot_cooked_path").setValue(v);
        v = ss_cfg::read("import_from");
        if (!v.empty()) cvarManager->getCvar("suitespot_import_from").setValue(v);
    }

    cvarManager->registerNotifier("suitespot_refresh_maps", [this](std::vector<std::string>) {
    std::error_code ec;
    ss_paths::ensureDataDirs(ec);
    if (ec) { LOG_WARN(cvarManager, "Failed to ensure data dirs"); }
    LOG_INFO(cvarManager, "Refresh maps requested");
}, "Refresh SuiteSpot maps", PERMISSION_ALL);

    cvarManager->registerNotifier("suitespot_open_workshop", [this](std::vector<std::string>) {
        auto path = cvarManager->getCvar("suitespot_workshop_path").getStringValue();
        // If no path is set, attempt to detect one from common locations
        if (path.empty()) {
            std::filesystem::path p = ss_epic::detectWorkshopRoot();
            if (!p.empty()) {
                cvarManager->getCvar("suitespot_workshop_path").setValue(p.string());
                path = p.string();
            }
        }
        if (!path.empty()) {
            std::string cmd = "start \"\" \"" + path + "\"";
            std::system(cmd.c_str());
            LOG_INFO(cvarManager, std::string("Opening folder: ") + path);
        } else {
            LOG_WARN(cvarManager, "No workshop path set or detected");
        }
    }, "Open workshop folder", PERMISSION_ALL);

    // Notifier: open the Epic CookedPCConsole directory specified by
    // suitespot_cooked_path. This command simply opens an Explorer window
    // pointing to the configured directory. If the path is empty or not a
    // directory, a warning is logged instead.
    cvarManager->registerNotifier("suitespot_open_cooked", [this](std::vector<std::string>) {
        auto cooked = cvarManager->getCvar("suitespot_cooked_path").getStringValue();
        if (!cooked.empty() && ss_epic::exists_dir(cooked)) {
            std::string cmd = "start \"\" \"" + cooked + "\"";
            std::system(cmd.c_str());
            LOG_INFO(cvarManager, std::string("Opening CookedPCConsole: ") + cooked);
        } else {
            LOG_WARN(cvarManager, "CookedPCConsole path not set or invalid");
        }
    }, "Open CookedPCConsole Directory", PERMISSION_ALL);

    // Notifier: download and install workshop textures into the cooked path. It
    // downloads a zip file using PowerShell and expands it into the cooked
    // directory. This operation may take some time and requires network
    // connectivity. The URL is hard-coded; adjust if necessary.
    cvarManager->registerNotifier("suitespot_download_textures", [this](std::vector<std::string>) {
        auto cooked = cvarManager->getCvar("suitespot_cooked_path").getStringValue();
        if (cooked.empty() || !ss_epic::exists_dir(cooked)) {
            LOG_ERR(cvarManager, "CookedPCConsole path not set or invalid.");
            return;
        }
        // Temporary file for downloaded zip
        const char* tmp = std::getenv("TEMP");
        std::string zip = std::string(tmp ? tmp : ".") + "\\suitespot_textures.zip";
        // URL of textures archive. This example uses JetFox's hosting as seen
        // in WorkshopMapLoader. Replace with your own mirror if required.
        const std::string url = "https://celab.jetfox.ovh/assets/textures/V1.0.0/textures.zip";
        LOG_INFO(cvarManager, "Downloading workshop textures...");
        if (!ss_epic::ps_download_to(url, zip)) {
            LOG_ERR(cvarManager, "Download failed; aborting install.");
            return;
        }
        LOG_INFO(cvarManager, "Extracting textures to CookedPCConsole...");
        ss_epic::ps_expand_zip(zip, cooked);
        if (ss_epic::texturesInstalled(cooked)) {
            LOG_INFO(cvarManager, "Workshop textures installed.");
            ss_cfg::write("textures_installed", "1");
        } else {
            LOG_WARN(cvarManager, "Textures installation incomplete; please verify required files.");
        }
    }, "Download & Install Workshop Textures", PERMISSION_ALL);

    // Notifier: import workshop maps from a folder into the cooked directory.
    // Files with .udk/.upk/.pak are copied; .zip archives are extracted.
    cvarManager->registerNotifier("suitespot_import_now", [this](std::vector<std::string>) {
        auto src = cvarManager->getCvar("suitespot_import_from").getStringValue();
        auto cooked = cvarManager->getCvar("suitespot_cooked_path").getStringValue();
        if (src.empty()) {
            LOG_WARN(cvarManager, "Import path not set.");
            return;
        }
        if (cooked.empty() || !ss_epic::exists_dir(cooked)) {
            LOG_ERR(cvarManager, "CookedPCConsole path not set or invalid.");
            return;
        }
        std::error_code ec;
        int copied = 0;
        int unzipped = 0;
        for (auto& entry : std::filesystem::directory_iterator(src, ec)) {
            if (ec) { ec.clear(); continue; }
            if (!entry.is_regular_file()) continue;
            auto p = entry.path();
            auto ext = p.extension().string();
            // lower-case extension
            std::string extL = ext;
            for (auto& c : extL) c = (char)tolower((unsigned char)c);
            if (extL == ".zip") {
                ss_epic::ps_expand_zip(p.string(), cooked);
                ++unzipped;
            } else if (extL == ".udk" || extL == ".upk" || extL == ".pak") {
                auto dst = std::filesystem::path(cooked) / p.filename();
                std::filesystem::copy_file(p, dst, std::filesystem::copy_options::overwrite_existing, ec);
                if (!ec) ++copied;
            }
        }
        LOG_INFO(cvarManager, std::string("Imported maps. Copied: ") + std::to_string(copied) + ", Unzipped: " + std::to_string(unzipped));
    }, "Import workshop maps from folder", PERMISSION_ALL);

// First-run: ensure data directories
{
    std::error_code ec;
    ss_paths::ensureDataDirs(ec);
    if (!ec) LOG_INFO(cvarManager, "Ensured SuiteTraining and SuiteWorkshops directories");
}

    _globalCvarManager = cvarManager;
    LOG("SuiteSpot loaded");
    LoadSettings();
    EnsureDataDirectories();
    EnsureReadmeFiles();
    LoadTrainingMaps();
    LoadWorkshopMaps();
    LoadHooks();

    // Expose one enable cvar to integrate with BakkesMod settings
    cvarManager->registerCvar("suitespot_enabled", "0", "Enable SuiteSpot", true, true, 0, true, 1)
        .addOnValueChanged([this](string oldValue, CVarWrapper cvar) {
            enabled = cvar.getBoolValue();
        });
    // Store training maps string for persistence compatibility
    cvarManager->registerCvar("ss_training_maps", "", "Stored training maps", true, false, 0, false, 0);
}

void SuiteSpot::onUnload() {
    SaveSettings();
    LOG("SuiteSpot unloaded");
}

// === Training auto-shuffle (minimal, training only) ===
void SuiteSpot::BuildTrainingShuffleBag() {
    trainingBag.clear();
    trainingBagPos = 0;
    if (RLTraining.empty()) return;
    trainingBag.reserve(RLTraining.size());
    for (size_t i=0;i<RLTraining.size();++i) trainingBag.push_back(i);
    // Fisher-Yates
    std::mt19937 rng{ (unsigned)time(nullptr) };
    for (size_t n = trainingBag.size(); n > 1; --n) {
        std::uniform_int_distribution<size_t> udist(0, n - 1);
        size_t j = udist(rng);
        std::swap(trainingBag[n-1], trainingBag[j]);
    }
}

// Returns next index from bag; rebuilds on exhaustion
int SuiteSpot::NextTrainingIndex() {
    if (RLTraining.empty()) return 0;
    if (trainingBagPos >= trainingBag.size()) BuildTrainingShuffleBag();
    if (trainingBag.empty()) return 0;
    return static_cast<int>( trainingBag[ trainingBagPos++ ] );
}