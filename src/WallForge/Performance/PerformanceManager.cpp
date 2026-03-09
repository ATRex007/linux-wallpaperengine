/**
 * WallForge - Dynamic Wallpaper Engine for Linux
 * Based on linux-wallpaperengine (GPLv3)
 *
 * PerformanceManager.cpp - Performance management implementation
 */

#include "PerformanceManager.h"

#include <iostream>
#include <algorithm>

namespace WallForge {

PerformanceManager::PerformanceManager() {
    m_state.active_profile = "balanced";
    m_state.current_fps = 30;
    m_state.effects_enabled = true;
    m_state.audio_enabled = true;
    m_state.is_paused = false;
}

PerformanceManager::~PerformanceManager() = default;

void PerformanceManager::init(const PerformanceConfig& config) {
    m_config = config;
    m_profiles = config.profiles;

    // Ensure default profiles exist
    if (m_profiles.find("high") == m_profiles.end())
        m_profiles["high"] = {60, true, true};
    if (m_profiles.find("balanced") == m_profiles.end())
        m_profiles["balanced"] = {30, true, true};
    if (m_profiles.find("low") == m_profiles.end())
        m_profiles["low"] = {15, false, false};

    setProfile(config.default_profile);

    // Setup battery callback
    if (config.battery.auto_switch) {
        m_battery.setOnPowerChange([this](const BatteryStatus& status) {
            checkBatteryState();
        });
    }

    std::cout << "[WallForge] Performance manager initialized, profile: "
              << m_state.active_profile << std::endl;
}

bool PerformanceManager::setProfile(const std::string& name) {
    auto it = m_profiles.find(name);
    if (it == m_profiles.end()) {
        std::cerr << "[WallForge] Unknown profile: " << name << std::endl;
        return false;
    }

    m_state.active_profile = name;
    applyProfile(it->second);
    std::cout << "[WallForge] Profile set: " << name
              << " (fps=" << m_state.current_fps
              << ", effects=" << m_state.effects_enabled
              << ", audio=" << m_state.audio_enabled << ")" << std::endl;

    if (m_stateChangeCallback) {
        m_stateChangeCallback(m_state);
    }

    return true;
}

std::vector<std::string> PerformanceManager::getProfileNames() const {
    std::vector<std::string> names;
    for (const auto& [name, _] : m_profiles) {
        names.push_back(name);
    }
    std::sort(names.begin(), names.end());
    return names;
}

PerformanceProfileConfig PerformanceManager::getProfile(const std::string& name) const {
    auto it = m_profiles.find(name);
    if (it != m_profiles.end()) {
        return it->second;
    }
    return {30, true, true}; // default fallback
}

void PerformanceManager::addAppRule(const AppRule& rule) {
    // Remove existing rule for same app
    removeAppRule(rule.app_name);
    m_appRules.push_back(rule);
    std::cout << "[WallForge] App rule added: " << rule.app_name
              << " -> " << rule.action << std::endl;
}

void PerformanceManager::removeAppRule(const std::string& appName) {
    m_appRules.erase(
        std::remove_if(m_appRules.begin(), m_appRules.end(),
                       [&](const AppRule& r) { return r.app_name == appName; }),
        m_appRules.end());
}

bool PerformanceManager::update() {
    bool changed = false;

    // Check battery state
    if (m_config.battery.auto_switch && m_battery.hasBattery()) {
        bool batteryChanged = m_battery.pollForChanges();
        if (batteryChanged) {
            checkBatteryState();
            changed = true;
        }
    }

    // App rules checked via external fullscreen detector integration
    // (uses linux-wallpaperengine's existing FullScreenDetector)

    return changed;
}

void PerformanceManager::setOnStateChange(StateChangeCallback callback) {
    m_stateChangeCallback = std::move(callback);
}

void PerformanceManager::pause(const std::string& reason) {
    if (!m_state.is_paused) {
        m_state.is_paused = true;
        m_state.pause_reason = reason;
        std::cout << "[WallForge] Paused: " << reason << std::endl;
        if (m_stateChangeCallback) {
            m_stateChangeCallback(m_state);
        }
    }
}

void PerformanceManager::resume() {
    if (m_state.is_paused) {
        m_state.is_paused = false;
        m_state.pause_reason = "";
        std::cout << "[WallForge] Resumed" << std::endl;
        if (m_stateChangeCallback) {
            m_stateChangeCallback(m_state);
        }
    }
}

void PerformanceManager::applyProfile(const PerformanceProfileConfig& profile) {
    m_state.current_fps = profile.max_fps;
    m_state.effects_enabled = profile.effects_enabled;
    m_state.audio_enabled = profile.audio_enabled;
}

void PerformanceManager::checkBatteryState() {
    auto status = m_battery.getStatus();

    if (status.state == PowerState::Battery) {
        // On battery: check if we need to switch profile
        if (status.percentage <= m_config.battery.critical_threshold) {
            // Critical battery level
            if (m_config.battery.critical_action == "pause") {
                pause("battery_critical");
            } else {
                setProfile("low");
            }
            std::cout << "[WallForge] Battery critical (" << status.percentage
                      << "%), action: " << m_config.battery.critical_action << std::endl;
        } else {
            // Normal battery mode: switch to battery profile
            setProfile(m_config.battery.on_battery_profile);
        }
    } else if (status.state == PowerState::AC) {
        // On AC: restore default profile
        if (m_state.is_paused && m_state.pause_reason == "battery_critical") {
            resume();
        }
        setProfile(m_config.default_profile);
    }
}

void PerformanceManager::checkAppRules() {
    // This will integrate with linux-wallpaperengine's fullscreen detection
    // For now, app rules are applied through the existing CLI flags
    // Full implementation will check running processes against rules
}

} // namespace WallForge
