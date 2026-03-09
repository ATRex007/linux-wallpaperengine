#pragma once

/**
 * WallForge - Dynamic Wallpaper Engine for Linux
 * Based on linux-wallpaperengine (GPLv3)
 *
 * PerformanceManager.h - Performance profile and resource management
 */

#include "BatteryDetector.h"
#include "../Core/Config.h"

#include <string>
#include <map>
#include <vector>
#include <functional>

namespace WallForge {

struct PerformanceState {
    std::string active_profile;
    int current_fps;
    bool effects_enabled;
    bool audio_enabled;
    bool is_paused;
    std::string pause_reason;     // "fullscreen", "battery_critical", "app_rule", ""
};

struct AppRule {
    std::string app_name;         // WM_CLASS or process name
    std::string condition;        // "fullscreen", "focused", "running"
    std::string action;           // "pause", "lower_fps", "mute", "change_profile"
    std::string action_value;     // action-specific value
    bool is_enabled = true;
};

class PerformanceManager {
public:
    using StateChangeCallback = std::function<void(const PerformanceState& state)>;

    PerformanceManager();
    ~PerformanceManager();

    /**
     * Initialize with config
     */
    void init(const PerformanceConfig& config);

    /**
     * Get current performance state
     */
    PerformanceState getState() const { return m_state; }

    /**
     * Set active profile by name ("high", "balanced", "low", or custom)
     */
    bool setProfile(const std::string& name);

    /**
     * Get available profile names
     */
    std::vector<std::string> getProfileNames() const;

    /**
     * Get profile config by name
     */
    PerformanceProfileConfig getProfile(const std::string& name) const;

    // ---- App Rules ----
    void addAppRule(const AppRule& rule);
    void removeAppRule(const std::string& appName);
    std::vector<AppRule> getAppRules() const { return m_appRules; }

    // ---- Battery Integration ----
    BatteryDetector& getBatteryDetector() { return m_battery; }

    /**
     * Called periodically to check battery, running apps, and adjust state.
     * Returns true if state changed.
     */
    bool update();

    /**
     * Register callback for state changes
     */
    void setOnStateChange(StateChangeCallback callback);

    /**
     * Manually pause/resume
     */
    void pause(const std::string& reason = "manual");
    void resume();
    bool isPaused() const { return m_state.is_paused; }

private:
    PerformanceState m_state;
    PerformanceConfig m_config;
    std::map<std::string, PerformanceProfileConfig> m_profiles;
    std::vector<AppRule> m_appRules;
    BatteryDetector m_battery;
    StateChangeCallback m_stateChangeCallback;

    void applyProfile(const PerformanceProfileConfig& profile);
    void checkBatteryState();
    void checkAppRules();
};

} // namespace WallForge
