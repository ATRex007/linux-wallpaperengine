/**
 * WallForge - Dynamic Wallpaper Engine for Linux
 * Based on linux-wallpaperengine (GPLv3)
 *
 * Config.cpp - Configuration management implementation
 */

#include "Config.h"

#include <fstream>
#include <iostream>

namespace WallForge {

// ---- JSON Serialization ----

void to_json(nlohmann::json& j, const LibraryConfig& c) {
    j = nlohmann::json{{"paths", c.paths}, {"database", c.database_path}};
}

void from_json(const nlohmann::json& j, LibraryConfig& c) {
    if (j.contains("paths")) j.at("paths").get_to(c.paths);
    if (j.contains("database")) j.at("database").get_to(c.database_path);
}

void to_json(nlohmann::json& j, const BatteryConfig& c) {
    j = nlohmann::json{
        {"auto_switch", c.auto_switch},
        {"on_battery_profile", c.on_battery_profile},
        {"critical_level_action", c.critical_action},
        {"critical_level_threshold", c.critical_threshold}
    };
}

void from_json(const nlohmann::json& j, BatteryConfig& c) {
    if (j.contains("auto_switch")) j.at("auto_switch").get_to(c.auto_switch);
    if (j.contains("on_battery_profile")) j.at("on_battery_profile").get_to(c.on_battery_profile);
    if (j.contains("critical_level_action")) j.at("critical_level_action").get_to(c.critical_action);
    if (j.contains("critical_level_threshold")) j.at("critical_level_threshold").get_to(c.critical_threshold);
}

void to_json(nlohmann::json& j, const PerformanceProfileConfig& c) {
    j = nlohmann::json{
        {"max_fps", c.max_fps},
        {"effects_enabled", c.effects_enabled},
        {"audio_enabled", c.audio_enabled}
    };
}

void from_json(const nlohmann::json& j, PerformanceProfileConfig& c) {
    if (j.contains("max_fps")) j.at("max_fps").get_to(c.max_fps);
    if (j.contains("effects_enabled")) j.at("effects_enabled").get_to(c.effects_enabled);
    if (j.contains("audio_enabled")) j.at("audio_enabled").get_to(c.audio_enabled);
}

void to_json(nlohmann::json& j, const PerformanceConfig& c) {
    j = nlohmann::json{
        {"default_profile", c.default_profile},
        {"profiles", c.profiles},
        {"battery", c.battery}
    };
}

void from_json(const nlohmann::json& j, PerformanceConfig& c) {
    if (j.contains("default_profile")) j.at("default_profile").get_to(c.default_profile);
    if (j.contains("profiles")) j.at("profiles").get_to(c.profiles);
    if (j.contains("battery")) j.at("battery").get_to(c.battery);
}

void to_json(nlohmann::json& j, const PlaylistConfig& c) {
    j = nlohmann::json{
        {"default_transition", c.default_transition},
        {"transition_duration_ms", c.transition_duration_ms}
    };
}

void from_json(const nlohmann::json& j, PlaylistConfig& c) {
    if (j.contains("default_transition")) j.at("default_transition").get_to(c.default_transition);
    if (j.contains("transition_duration_ms")) j.at("transition_duration_ms").get_to(c.transition_duration_ms);
}

void to_json(nlohmann::json& j, const DisplayConfig& c) {
    j = nlohmann::json{{"backend", c.backend}};
}

void from_json(const nlohmann::json& j, DisplayConfig& c) {
    if (j.contains("backend")) j.at("backend").get_to(c.backend);
}

void to_json(nlohmann::json& j, const WallForgeConfig& c) {
    j = nlohmann::json{
        {"version", c.version},
        {"library", c.library},
        {"performance", c.performance},
        {"playlist", c.playlist},
        {"display", c.display}
    };
}

void from_json(const nlohmann::json& j, WallForgeConfig& c) {
    if (j.contains("version")) j.at("version").get_to(c.version);
    if (j.contains("library")) j.at("library").get_to(c.library);
    if (j.contains("performance")) j.at("performance").get_to(c.performance);
    if (j.contains("playlist")) j.at("playlist").get_to(c.playlist);
    if (j.contains("display")) j.at("display").get_to(c.display);
}

// ---- Config Implementation ----

Config& Config::getInstance() {
    static Config instance;
    return instance;
}

std::filesystem::path Config::getDefaultConfigPath() {
    const char* xdgConfig = std::getenv("XDG_CONFIG_HOME");
    std::filesystem::path configDir;
    if (xdgConfig) {
        configDir = std::filesystem::path(xdgConfig) / "wallforge";
    } else {
        const char* home = std::getenv("HOME");
        configDir = std::filesystem::path(home ? home : "/tmp") / ".config" / "wallforge";
    }
    return configDir / "config.json";
}

std::filesystem::path Config::getDefaultDataDir() {
    const char* xdgData = std::getenv("XDG_DATA_HOME");
    if (xdgData) {
        return std::filesystem::path(xdgData) / "wallforge";
    }
    const char* home = std::getenv("HOME");
    return std::filesystem::path(home ? home : "/tmp") / ".local" / "share" / "wallforge";
}

std::filesystem::path Config::getDefaultDatabasePath() {
    return getDefaultDataDir() / "library.db";
}

std::filesystem::path Config::findSteamRoot() {
    const char* home = std::getenv("HOME");
    if (!home) return {};

    // Check common Steam installation paths
    static const std::vector<std::string> steamRoots = {
        std::string(home) + "/.local/share/Steam",
        std::string(home) + "/.steam/steam",
        std::string(home) + "/.steam/debian-installation",
    };

    for (const auto& root : steamRoots) {
        if (std::filesystem::exists(root + "/steamapps")) {
            return root;
        }
    }
    return {};
}

void Config::initDefaults() {
    m_config.version = 1;

    // Default library paths - auto-detect Steam installation
    const char* home = std::getenv("HOME");
    std::string homeStr = home ? home : "/tmp";

    auto steamRoot = findSteamRoot();
    m_config.library.paths = {};

    if (!steamRoot.empty()) {
        auto workshopDir = steamRoot / "steamapps" / "workshop" / "content" / "431960";
        auto defaultDir = steamRoot / "steamapps" / "common" / "wallpaper_engine" / "projects" / "defaultprojects";

        if (std::filesystem::exists(workshopDir)) {
            m_config.library.paths.push_back(workshopDir.string());
        }
        if (std::filesystem::exists(defaultDir)) {
            m_config.library.paths.push_back(defaultDir.string());
        }
    }

    m_config.library.paths.push_back(homeStr + "/Pictures/Wallpapers");
    m_config.library.database_path = getDefaultDatabasePath().string();

    // Default performance profiles
    m_config.performance.default_profile = "balanced";
    m_config.performance.profiles["high"] = {60, true, true};
    m_config.performance.profiles["balanced"] = {30, true, true};
    m_config.performance.profiles["low"] = {15, false, false};

    // Default battery config
    m_config.performance.battery.auto_switch = true;
    m_config.performance.battery.on_battery_profile = "low";
    m_config.performance.battery.critical_action = "pause";
    m_config.performance.battery.critical_threshold = 15;

    // Default playlist config
    m_config.playlist.default_transition = "crossfade";
    m_config.playlist.transition_duration_ms = 1000;

    // Default display config
    m_config.display.backend = "x11";
}

bool Config::load(const std::filesystem::path& path) {
    m_configPath = path;
    initDefaults();

    if (!std::filesystem::exists(path)) {
        std::cout << "[WallForge] Config not found, creating defaults at: " << path << std::endl;
        std::filesystem::create_directories(path.parent_path());
        return save();
    }

    try {
        std::ifstream file(path);
        nlohmann::json j;
        file >> j;
        m_config = j.get<WallForgeConfig>();
        std::cout << "[WallForge] Config loaded from: " << path << std::endl;
        return true;
    } catch (const std::exception& e) {
        std::cerr << "[WallForge] Error loading config: " << e.what() << std::endl;
        return false;
    }
}

bool Config::save() const {
    try {
        std::filesystem::create_directories(m_configPath.parent_path());
        std::ofstream file(m_configPath);
        nlohmann::json j = m_config;
        file << j.dump(2);
        std::cout << "[WallForge] Config saved to: " << m_configPath << std::endl;
        return true;
    } catch (const std::exception& e) {
        std::cerr << "[WallForge] Error saving config: " << e.what() << std::endl;
        return false;
    }
}

} // namespace WallForge
