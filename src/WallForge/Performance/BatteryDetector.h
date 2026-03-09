#pragma once

/**
 * WallForge - Dynamic Wallpaper Engine for Linux
 * Based on linux-wallpaperengine (GPLv3)
 *
 * BatteryDetector.h - Battery status detection via UPower D-Bus
 */

#include <string>
#include <functional>

namespace WallForge {

enum class PowerState {
    AC,           // 電源接続中
    Battery,      // バッテリー駆動
    Unknown
};

struct BatteryStatus {
    PowerState state = PowerState::Unknown;
    double percentage = 100.0;      // 0-100
    bool is_charging = false;
    int time_to_empty_seconds = -1; // 残り時間（秒）、不明の場合-1
};

/**
 * Detects battery status using /sys/class/power_supply
 * Falls back to reading sysfs directly (no D-Bus dependency for initial version)
 */
class BatteryDetector {
public:
    using PowerChangeCallback = std::function<void(const BatteryStatus& status)>;

    BatteryDetector();
    ~BatteryDetector();

    /**
     * Check if the system has a battery
     */
    bool hasBattery() const;

    /**
     * Get current battery status
     */
    BatteryStatus getStatus() const;

    /**
     * Check if currently on AC power
     */
    bool isOnAC() const;

    /**
     * Get battery percentage (0-100)
     */
    double getBatteryLevel() const;

    /**
     * Register callback for power state changes
     */
    void setOnPowerChange(PowerChangeCallback callback);

    /**
     * Poll for changes. Should be called periodically.
     * Returns true if state changed since last poll.
     */
    bool pollForChanges();

private:
    bool m_hasBattery = false;
    std::string m_batteryPath;       // e.g., /sys/class/power_supply/BAT0
    std::string m_acPath;            // e.g., /sys/class/power_supply/AC0
    BatteryStatus m_lastStatus;
    PowerChangeCallback m_callback;

    void detectPowerSupplyPaths();
    std::string readSysFile(const std::string& path) const;
    BatteryStatus readStatusFromSysfs() const;
};

} // namespace WallForge
