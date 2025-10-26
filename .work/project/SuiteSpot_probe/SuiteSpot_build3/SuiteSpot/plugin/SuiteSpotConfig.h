// SuiteSpotConfig.h
//
// Provides simple key=value configuration storage for SuiteSpot. Configuration
// values are persisted under the BakkesMod data directory in a
// `SuiteSpot` subfolder. This avoids any dependency on other plugins (such as
// WorkshopMapLoader) for configuration storage and allows SuiteSpot to
// remember user preferences between sessions.

#pragma once

#include <filesystem>
#include <string>
#include <vector>

// Namespace for SuiteSpot configuration helpers. These functions are
// intentionally lightweight and do not depend on external libraries. They
// read and write a simple key=value file located in
// `%AppData%/bakkesmod/bakkesmod/data/SuiteSpot/suitespot.cfg`.
namespace ss_cfg {
    namespace fs = std::filesystem;

    // Returns the root of the BakkesMod data directory. If the APPDATA
    // environment variable is unavailable, an empty path is returned.
    inline fs::path bmDataRoot() {
        const char* app = std::getenv("APPDATA");
        return app ? fs::path(app) / "bakkesmod" / "bakkesmod" / "data" : fs::path{};
    }

    // Returns the SuiteSpot-specific data directory. This is where the
    // configuration file lives. The directory is not created automatically;
    // callers should ensure it exists before writing.
    inline fs::path suiteSpotDataDir() { return bmDataRoot() / "SuiteSpot"; }

    // Returns the full path to the configuration file. The file may not
    // exist; callers should handle that case appropriately.
    inline fs::path suiteSpotCfgPath() { return suiteSpotDataDir() / "suitespot.cfg"; }

    // Reads the value for a given key from the configuration file. If the
    // key is not present, an empty string is returned. The key search is
    // case-sensitive. This function creates no files or directories.
    std::string read(const std::string& key);

    // Writes (or updates) a key=value pair in the configuration file.
    // If the file or its parent directory does not exist, they will be
    // created. If the key already exists in the file, its value will be
    // overwritten. Otherwise, the key/value pair is appended at the end of
    // the file. Keys are case-sensitive.
    void write(const std::string& key, const std::string& val);
}
