
#include "pch.h"
#include "SuiteSpot.h"
#include "MapList.h"
#include <fstream>
#include <sstream>

void SuiteSpot::RenderSettings() {
    ImGui::TextUnformatted("QuickSuite Settings"); // keep user's label
    
    // 1) Enable QuickSuite (checkbox)
    CVarWrapper enableCvar = cvarManager->getCvar("suitespot_enabled");
    if (!enableCvar) return;
    enabled = enableCvar.getBoolValue();
    if (ImGui::Checkbox("Enable QuickSuite", &enabled)) {
        enableCvar.setValue(enabled);
        SaveSettings();
    }

    ImGui::Separator(); // --------------------------------

    // 2) Select Map Type (buttons)
    ImGui::TextUnformatted("Select Map Type");
    const char* mapLabels[] = {"Freeplay","Training","Workshop"};
    for (int i=0;i<3;i++) {
        ImGui::SameLine(i==0?0.0f:0.0f);
        if (ImGui::RadioButton(mapLabels[i], mapType==i)) { mapType=i; SaveSettings(); }
        if (i<2) ImGui::SameLine();
    }

    ImGui::Separator(); // --------------------------------

    // 3) Auto-Queuing Active + Delay Queue (sec)
    if (ImGui::Checkbox("Auto-Queuing Active", &autoQueue)) {
        SaveSettings();
    }
    ImGui::SetNextItemWidth(220);
    if (ImGui::InputInt("Delay Queue (sec)", &delayQueueSec)) {
        delayQueueSec = std::max(0, delayQueueSec);
        SaveSettings();
    }

    ImGui::Separator(); // --------------------------------

    // 4) Training Packs / Workshop maps dropdown (interchangeable based on map type)
    if (mapType == 1) {
        if (ImGui::BeginCombo("Training Packs", (RLTraining.empty()?"<none>":RLTraining[currentTrainingIndex].name.c_str()))) {
            for (int i=0;i<(int)RLTraining.size();++i) {
                bool selected = (i==currentTrainingIndex);
                if (ImGui::Selectable(RLTraining[i].name.c_str(), selected)) { currentTrainingIndex = i; SaveSettings(); }
            }
            ImGui::EndCombo();
        }
        ImGui::SameLine();
        bool _ss_shuffle = trainingShuffleEnabled;
        if (ImGui::Checkbox("Auto-Shuffle##train", &_ss_shuffle)) { trainingShuffleEnabled = _ss_shuffle; if (trainingShuffleEnabled) { BuildTrainingShuffleBag(); } }
        static char newMapCode[64] = {0};
        static char newMapName[64] = {0};
        ImGui::InputText("Training Map Code", newMapCode, IM_ARRAYSIZE(newMapCode));
        ImGui::InputText("Training Map Name", newMapName, IM_ARRAYSIZE(newMapName));
        if (ImGui::Button("Add Training Map")) {
            if (strlen(newMapCode) > 0 && strlen(newMapName) > 0) {
                RLTraining.push_back({ std::string(newMapCode), std::string(newMapName) });
                // replaced by persistent storage
                SaveTrainingMaps();
                // legacy file write removed
                
                newMapCode[0] = 0; newMapName[0] = 0;
            }
        }
    
    } else if (mapType == 2) {
        if (ImGui::BeginCombo("Workshop Maps", (RLWorkshop.empty()?"<none>":RLWorkshop[currentWorkshopIndex].name.c_str()))) {
            for (int i=0;i<(int)RLWorkshop.size();++i) {
                bool selected = (i==currentWorkshopIndex);
                if (ImGui::Selectable(RLWorkshop[i].name.c_str(), selected)) { currentWorkshopIndex = i; SaveSettings(); }
                if (selected) ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
        ImGui::SameLine();
        if (ImGui::Button("Rescan##ws")) { LoadWorkshopMaps(); SaveSettings(); }
        ImGui::SameLine();
        ImGui::TextDisabled("(%d found)", (int)RLWorkshop.size());
        // Display path hint
        ImGui::TextWrapped("Workshop maps are discovered from Epic/Steam mods folders (recursive).");
    }


    ImGui::Separator(); // --------------------------------

    // 5) Delays per mode
    ImGui::SetNextItemWidth(220);
    if (ImGui::InputInt("Delay Freeplay (sec)", &delayFreeplaySec)) { delayFreeplaySec = std::max(0, delayFreeplaySec); SaveSettings(); }
    ImGui::SetNextItemWidth(220);
    if (ImGui::InputInt("Delay Training (sec)", &delayTrainingSec)) { delayTrainingSec = std::max(0, delayTrainingSec); SaveSettings(); }
    ImGui::SetNextItemWidth(220);
    if (ImGui::InputInt("Delay Workshop (sec)", &delayWorkshopSec)) { delayWorkshopSec = std::max(0, delayWorkshopSec); SaveSettings(); }
}
