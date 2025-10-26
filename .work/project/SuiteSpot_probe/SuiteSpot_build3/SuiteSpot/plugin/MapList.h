#pragma once
#include <string>
#include <vector>

// Freeplay maps
struct MapEntry {
    std::string code;
    std::string name;
};
extern std::vector<MapEntry> RLMaps;

// Training packs
struct TrainingEntry {
    std::string code;
    std::string name;
};
extern std::vector<TrainingEntry> RLTraining;

// Workshop maps
struct WorkshopEntry {
    std::string filePath;
    std::string name;      
};
extern std::vector<WorkshopEntry> RLWorkshop;
