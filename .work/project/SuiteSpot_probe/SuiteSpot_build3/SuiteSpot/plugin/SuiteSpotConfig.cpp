// SuiteSpotConfig.cpp
//
// Implementation of SuiteSpot configuration helpers defined in
// SuiteSpotConfig.h. Uses a simple key=value format with one entry per
// line. Lines beginning with '#' are treated as comments and ignored. The
// file is stored under `%AppData%/bakkesmod/bakkesmod/data/SuiteSpot`.

#include "SuiteSpotConfig.h"
#include <fstream>
#include <system_error>

namespace ss_cfg {

    // Ensures that the SuiteSpot data directory exists. The directory is
    // created if it does not already exist. Any errors are silently
    // ignored; callers should handle missing directories during file I/O.
    static void ensureDir() {
        std::error_code ec;
        fs::create_directories(suiteSpotDataDir(), ec);
    }

    // Reads all key=value entries from the configuration file into a vector
    // of pairs. If the file cannot be opened, an empty vector is returned.
    static std::vector<std::pair<std::string, std::string>> parseAll() {
        std::vector<std::pair<std::string, std::string>> kv;
        const auto cfg = suiteSpotCfgPath();
        std::ifstream in(cfg.string());
        if (!in.is_open()) return kv;
        std::string line;
        while (std::getline(in, line)) {
            // Skip comments and empty lines
            if (line.empty() || line[0] == '#') continue;
            auto pos = line.find('=');
            if (pos == std::string::npos) continue;
            std::string key = line.substr(0, pos);
            std::string val = line.substr(pos + 1);
            kv.push_back({ key, val });
        }
        return kv;
    }

    std::string read(const std::string& key) {
        auto entries = parseAll();
        for (const auto& [k, v] : entries) {
            if (k == key) return v;
        }
        return "";
    }

    void write(const std::string& key, const std::string& val) {
        ensureDir();
        auto entries = parseAll();
        bool found = false;
        for (auto& [k, v] : entries) {
            if (k == key) {
                v = val;
                found = true;
                break;
            }
        }
        if (!found) {
            entries.push_back({ key, val });
        }
        // Write back all entries
        std::ofstream out(suiteSpotCfgPath().string(), std::ios::trunc);
        if (!out.is_open()) return;
        for (const auto& [k, v] : entries) {
            out << k << '=' << v << '\n';
        }
    }

} // namespace ss_cfg
