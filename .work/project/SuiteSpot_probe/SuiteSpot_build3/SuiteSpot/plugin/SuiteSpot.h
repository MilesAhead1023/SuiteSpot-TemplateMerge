#pragma once
#include "logging.h"
#include "SuiteSpotConfig.h"
#include "GuiBase.h" // defines SettingsWindowBase
#include "bakkesmod/plugin/bakkesmodplugin.h"
#include "bakkesmod/plugin/pluginwindow.h"
#include "bakkesmod/plugin/PluginSettingsWindow.h"
#include "MapList.h"
#include "version.h"
#include <filesystem>
#include <vector>

// External helpers
void SaveTrainingMaps(std::shared_ptr<CVarManagerWrapper> cvarManager, const std::vector<TrainingEntry>& RLTraining);
void LoadTrainingMaps(std::shared_ptr<CVarManagerWrapper> cvarManager, std::vector<TrainingEntry>& RLTraining);

// Version macro carried over from the master template
constexpr auto plugin_version =
    stringify(VERSION_MAJOR) "."
    stringify(VERSION_MINOR) "."
    stringify(VERSION_PATCH) "."
    stringify(VERSION_BUILD);

// NOTE: inherit from SettingsWindowBase (not “GuiBase”)
class SuiteSpot final : public BakkesMod::Plugin::BakkesModPlugin,
                        public SettingsWindowBase
{
public:
    // Persistence folders and files under %APPDATA%\bakkesmod\bakkesmod\data
    void EnsureDataDirectories() const;
    std::filesystem::path GetDataRoot() const;
    std::filesystem::path GetSuiteTrainingDir() const;
    std::filesystem::path GetSuiteWorkshopsDir() const;
    std::filesystem::path GetTrainingFilePath() const;   // SuiteTraining\SuiteSpotTrainingMaps.txt
    std::filesystem::path GetWorkshopFilePath() const;   // SuiteWorkshops\SuiteSpotWorkshopMaps.txt

    // Persistence API
    void LoadTrainingMaps();
    void SaveTrainingMaps() const;
    void LoadWorkshopMaps();        // extends previous disk-scan to also read/write file
    void SaveWorkshopMaps() const; // no-op (legacy)
    void DiscoverWorkshopInDir(const std::filesystem::path& dir);
// File/dir utilities
void MirrorDirectory(const std::filesystem::path& src, const std::filesystem::path& dst) const;
void EnsureReadmeFiles() const;

    // lifecycle
    void onLoad() override;
    void onUnload() override;

    // settings UI
    void RenderSettings() override;

    // hooks
    void LoadHooks();
    void GameEndedEvent(std::string name);

    // persistence
    void SaveSettings();
    void LoadSettings();

    // workshop discovery (needed because SuiteSpot.cpp calls it)
// (removed duplicate)

private:
    // state (one definition only)
    bool enabled = false;

    bool autoQueue = false;   // (renamed from “Requeue”)
    int  mapType = 0;         // 0=Freeplay, 1=Training, 2=Workshop

    int  delayQueueSec    = 0;
    int  delayFreeplaySec = 0;
    int  delayTrainingSec = 0;
    int  delayWorkshopSec = 0;

    int  currentIndex = 0;           // freeplay
    int  currentTrainingIndex = 0;   // training
    int  currentWorkshopIndex = 0;   // workshop

    // Auto-shuffle for training maps (minimal, in-memory)
    bool trainingShuffleEnabled = false;
    std::vector<size_t> trainingBag;
    size_t trainingBagPos = 0;

    std::string lastGameMode = "";

    // helpers
    void BuildTrainingShuffleBag();
    int  NextTrainingIndex();
};