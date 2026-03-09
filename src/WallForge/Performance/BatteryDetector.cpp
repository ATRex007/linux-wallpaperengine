/**
 * WallForge - Dynamic Wallpaper Engine for Linux
 * Based on linux-wallpaperengine (GPLv3)
 *
 * BatteryDetector.cpp - Battery detection via sysfs
 */

#include "BatteryDetector.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <algorithm>

namespace WallForge {

BatteryDetector::BatteryDetector() {
    detectPowerSupplyPaths();
    if (m_hasBattery) {
        m_lastStatus = readStatusFromSysfs();
    }
}

BatteryDetector::~BatteryDetector() = default;

void BatteryDetector::detectPowerSupplyPaths() {
    const std::filesystem::path sysPath = "/sys/class/power_supply";

    if (!std::filesystem::exists(sysPath)) {
        m_hasBattery = false;
        return;
    }

    for (const auto& entry : std::filesystem::directory_iterator(sysPath)) {
        std::string type = readSysFile(entry.path() / "type");
        // Remove trailing whitespace/newline
        type.erase(std::remove_if(type.begin(), type.end(), ::isspace), type.end());

        if (type == "Battery") {
            m_batteryPath = entry.path().string();
            m_hasBattery = true;
        } else if (type == "Mains") {
            m_acPath = entry.path().string();
        }
    }

    if (m_hasBattery) {
        std::cout << "[WallForge] Battery detected: " << m_batteryPath << std::endl;
    } else {
        std::cout << "[WallForge] No battery detected (desktop system)" << std::endl;
    }
}

bool BatteryDetector::hasBattery() const {
    return m_hasBattery;
}

BatteryStatus BatteryDetector::getStatus() const {
    if (!m_hasBattery) {
        BatteryStatus status;
        status.state = PowerState::AC;
        status.percentage = 100.0;
        return status;
    }
    return readStatusFromSysfs();
}

bool BatteryDetector::isOnAC() const {
    auto status = getStatus();
    return status.state == PowerState::AC;
}

double BatteryDetector::getBatteryLevel() const {
    auto status = getStatus();
    return status.percentage;
}

void BatteryDetector::setOnPowerChange(PowerChangeCallback callback) {
    m_callback = std::move(callback);
}

bool BatteryDetector::pollForChanges() {
    if (!m_hasBattery) return false;

    auto current = readStatusFromSysfs();
    bool changed = (current.state != m_lastStatus.state) ||
                   (std::abs(current.percentage - m_lastStatus.percentage) > 1.0);

    if (changed) {
        m_lastStatus = current;
        if (m_callback) {
            m_callback(current);
        }
    }

    return changed;
}

std::string BatteryDetector::readSysFile(const std::string& path) const {
    std::ifstream file(path);
    if (!file.is_open()) return "";
    std::string content;
    std::getline(file, content);
    return content;
}

BatteryStatus BatteryDetector::readStatusFromSysfs() const {
    BatteryStatus status;

    if (!m_hasBattery) {
        status.state = PowerState::AC;
        status.percentage = 100.0;
        return status;
    }

    // Read capacity (0-100)
    std::string capacityStr = readSysFile(m_batteryPath + "/capacity");
    if (!capacityStr.empty()) {
        try {
            status.percentage = std::stod(capacityStr);
        } catch (...) {
            status.percentage = -1;
        }
    }

    // Read status (Charging, Discharging, Full, Not charging)
    std::string statusStr = readSysFile(m_batteryPath + "/status");
    statusStr.erase(std::remove_if(statusStr.begin(), statusStr.end(), ::isspace), statusStr.end());

    if (statusStr == "Charging") {
        status.state = PowerState::AC;
        status.is_charging = true;
    } else if (statusStr == "Discharging") {
        status.state = PowerState::Battery;
        status.is_charging = false;
    } else if (statusStr == "Full" || statusStr == "Notcharging") {
        status.state = PowerState::AC;
        status.is_charging = false;
    } else {
        // Fallback: check AC adapter
        if (!m_acPath.empty()) {
            std::string online = readSysFile(m_acPath + "/online");
            online.erase(std::remove_if(online.begin(), online.end(), ::isspace), online.end());
            status.state = (online == "1") ? PowerState::AC : PowerState::Battery;
        } else {
            status.state = PowerState::Unknown;
        }
    }

    // Read time to empty
    std::string energyNow = readSysFile(m_batteryPath + "/energy_now");
    std::string powerNow = readSysFile(m_batteryPath + "/power_now");
    if (!energyNow.empty() && !powerNow.empty()) {
        try {
            double energy = std::stod(energyNow);
            double power = std::stod(powerNow);
            if (power > 0) {
                status.time_to_empty_seconds = static_cast<int>((energy / power) * 3600);
            }
        } catch (...) {}
    }

    return status;
}

} // namespace WallForge
