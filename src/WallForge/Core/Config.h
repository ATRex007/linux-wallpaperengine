#pragma once

/**
 * WallForge - Dynamic Wallpaper Engine for Linux
 * Based on linux-wallpaperengine (GPLv3)
 *
 * Config.h - Configuration management
 */

#include <string>
#include <filesystem>
#include <nlohmann/json.hpp>

namespace WallForge {

struct LibraryConfig {
    std::vector<std::string> paths;
    std::string database_path;
};

struct BatteryConfig {
    bool auto_switch = true;
    std::string on_battery_profile = "low";
    std::string critical_action = "pause";
    int critical_threshold = 15;
};

struct PerformanceProfileConfig {
    int max_fps = 60;
    bool effects_enabled = true;
    bool audio_enabled = true;
};

struct PerformanceConfig {
    std::string default_profile = "balanced";
    std::map<std::string, PerformanceProfileConfig> profiles;
    BatteryConfig battery;
};

struct PlaylistConfig {
    std::string default_transition = "crossfade";
    int transition_duration_ms = 1000;
};

struct DisplayConfig {
    std::string backend = "x11";  // "x11" or "wayland"
};

struct WallForgeConfig {
    int version = 1;
    LibraryConfig library;
    PerformanceConfig performance;
    PlaylistConfig playlist;
    DisplayConfig display;
};

class Config {
public:
    /**
     * Load configuration from file.
     * If file doesn't exist, creates default config.
     */
    static Config& getInstance();

    bool load(const std::filesystem::path& path = getDefaultConfigPath());
    bool save() const;

    WallForgeConfig& get() { return m_config; }
    const WallForgeConfig& get() const { return m_config; }

    static std::filesystem::path getDefaultConfigPath();
    static std::filesystem::path getDefaultDataDir();
    static std::filesystem::path getDefaultDatabasePath();
    static std::filesystem::path findSteamRoot();

private:
    Config() = default;
    void initDefaults();

    WallForgeConfig m_config;
    std::filesystem::path m_configPath;
};

// JSON serialization
void to_json(nlohmann::json& j, const LibraryConfig& c);
void from_json(const nlohmann::json& j, LibraryConfig& c);
void to_json(nlohmann::json& j, const BatteryConfig& c);
void from_json(const nlohmann::json& j, BatteryConfig& c);
void to_json(nlohmann::json& j, const PerformanceProfileConfig& c);
void from_json(const nlohmann::json& j, PerformanceProfileConfig& c);
void to_json(nlohmann::json& j, const PerformanceConfig& c);
void from_json(const nlohmann::json& j, PerformanceConfig& c);
void to_json(nlohmann::json& j, const PlaylistConfig& c);
void from_json(const nlohmann::json& j, PlaylistConfig& c);
void to_json(nlohmann::json& j, const DisplayConfig& c);
void from_json(const nlohmann::json& j, DisplayConfig& c);
void to_json(nlohmann::json& j, const WallForgeConfig& c);
void from_json(const nlohmann::json& j, WallForgeConfig& c);

} // namespace WallForge
