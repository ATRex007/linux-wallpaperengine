/**
 * WallForge - Dynamic Wallpaper Engine for Linux
 * Based on linux-wallpaperengine (GPLv3)
 *
 * LibraryManager.cpp - Wallpaper library management implementation
 */

#include "LibraryManager.h"

#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>

namespace WallForge {

LibraryManager::LibraryManager(LibraryDatabase& db) : m_db(db) {}

int LibraryManager::scanDirectories(const std::vector<std::string>& paths, ProgressCallback progress) {
    int total = 0;
    for (const auto& path : paths) {
        std::filesystem::path p(path);
        // Expand ~ to home directory
        if (!p.string().empty() && p.string()[0] == '~') {
            const char* home = std::getenv("HOME");
            if (home) {
                p = std::filesystem::path(home) / p.string().substr(2);
            }
        }

        if (std::filesystem::exists(p) && std::filesystem::is_directory(p)) {
            total += scanDirectory(p, progress);
        } else {
            std::cerr << "[WallForge] Directory not found: " << p << std::endl;
        }
    }
    return total;
}

int LibraryManager::scanDirectory(const std::filesystem::path& dirPath, ProgressCallback progress) {
    int scanned = 0;
    int added = 0;

    try {
        for (const auto& entry : std::filesystem::directory_iterator(dirPath)) {
            if (!entry.is_directory()) continue;

            scanned++;
            if (progress) {
                progress(entry.path().string(), scanned, -1); // -1 = unknown total
            }

            if (isWallpaperDirectory(entry.path())) {
                auto projectJson = entry.path() / "project.json";

                // Check if already in database
                auto existing = m_db.getWallpaperByPath(entry.path().string());
                if (existing.has_value()) continue;

                try {
                    auto metadata = parseProjectJson(projectJson);
                    metadata.path = entry.path().string();
                    metadata.preview_path = findPreviewImage(entry.path()).string();
                    metadata.file_size_bytes = calculateDirectorySize(entry.path());

                    int64_t id = m_db.addWallpaper(metadata);
                    if (id > 0) {
                        added++;
                        std::cout << "[WallForge] Added: " << metadata.name
                                  << " (" << metadata.type << ")" << std::endl;
                    }
                } catch (const std::exception& e) {
                    std::cerr << "[WallForge] Error parsing " << projectJson
                              << ": " << e.what() << std::endl;
                }
            }
        }
    } catch (const std::filesystem::filesystem_error& e) {
        std::cerr << "[WallForge] Error scanning directory: " << e.what() << std::endl;
    }

    std::cout << "[WallForge] Scan complete: " << added << " new wallpapers found in "
              << dirPath << std::endl;
    return added;
}

int64_t LibraryManager::importFromPath(const std::filesystem::path& path) {
    if (!std::filesystem::exists(path)) {
        std::cerr << "[WallForge] Path does not exist: " << path << std::endl;
        return -1;
    }

    std::filesystem::path wpDir = path;
    if (std::filesystem::is_regular_file(path)) {
        wpDir = path.parent_path();
    }

    auto projectJson = wpDir / "project.json";
    if (!std::filesystem::exists(projectJson)) {
        std::cerr << "[WallForge] No project.json found in: " << wpDir << std::endl;
        return -1;
    }

    auto existing = m_db.getWallpaperByPath(wpDir.string());
    if (existing.has_value()) {
        std::cout << "[WallForge] Wallpaper already in library: " << existing->name << std::endl;
        return existing->id;
    }

    auto metadata = parseProjectJson(projectJson);
    metadata.path = wpDir.string();
    metadata.preview_path = findPreviewImage(wpDir).string();
    metadata.file_size_bytes = calculateDirectorySize(wpDir);

    return m_db.addWallpaper(metadata);
}

int LibraryManager::importFromSteam(const std::filesystem::path& steamContentDir) {
    std::filesystem::path contentDir = steamContentDir;
    if (contentDir.empty()) {
        const char* home = std::getenv("HOME");
        if (!home) return 0;

        // Check common Steam paths
        static const std::vector<std::string> steamRoots = {
            std::string(home) + "/.local/share/Steam",
            std::string(home) + "/.steam/steam",
            std::string(home) + "/.steam/debian-installation",
        };

        for (const auto& root : steamRoots) {
            auto candidate = std::filesystem::path(root) / "steamapps" / "workshop" / "content" / "431960";
            if (std::filesystem::exists(candidate)) {
                contentDir = candidate;
                break;
            }
        }
    }

    int total = 0;

    // Scan workshop content
    if (!contentDir.empty() && std::filesystem::exists(contentDir)) {
        std::cout << "[WallForge] Scanning Steam Workshop content: " << contentDir << std::endl;
        total += scanDirectory(contentDir);
    } else {
        std::cerr << "[WallForge] Steam Workshop directory not found" << std::endl;
    }

    // Also scan default wallpaper projects
    for (const auto& root : {std::string(std::getenv("HOME") ? std::getenv("HOME") : "") + "/.local/share/Steam",
                              std::string(std::getenv("HOME") ? std::getenv("HOME") : "") + "/.steam/steam"}) {
        auto defaultDir = std::filesystem::path(root) / "steamapps" / "common" / "wallpaper_engine" / "projects" / "defaultprojects";
        if (std::filesystem::exists(defaultDir)) {
            std::cout << "[WallForge] Scanning default wallpapers: " << defaultDir << std::endl;
            total += scanDirectory(defaultDir);
            break;
        }
    }

    return total;
}

std::string LibraryManager::detectWallpaperType(const std::filesystem::path& projectJsonPath) {
    try {
        std::ifstream file(projectJsonPath);
        nlohmann::json j;
        file >> j;

        if (j.contains("type")) {
            std::string type = j["type"].get<std::string>();
            // Normalize type string
            if (type == "scene") return "scene";
            if (type == "video") return "video";
            if (type == "web") return "web";
            // Lowercase check
            std::transform(type.begin(), type.end(), type.begin(), ::tolower);
            return type;
        }
    } catch (...) {}
    return "";
}

WallpaperMetadata LibraryManager::parseProjectJson(const std::filesystem::path& projectJsonPath) {
    WallpaperMetadata metadata;

    std::ifstream file(projectJsonPath);
    if (!file.is_open()) {
        throw std::runtime_error("Cannot open: " + projectJsonPath.string());
    }

    nlohmann::json j;
    file >> j;

    // Extract basic metadata
    if (j.contains("title")) {
        metadata.name = j["title"].get<std::string>();
    } else {
        metadata.name = projectJsonPath.parent_path().filename().string();
    }

    if (j.contains("type")) {
        metadata.type = j["type"].get<std::string>();
        std::transform(metadata.type.begin(), metadata.type.end(),
                       metadata.type.begin(), ::tolower);
    }

    // If type is missing, try to infer from file extension
    if (metadata.type.empty() && j.contains("file")) {
        std::string file_field = j["file"].get<std::string>();
        if (file_field.ends_with(".json") || file_field.ends_with(".pkg")) {
            metadata.type = "scene";  // Scene wallpapers use .json
        } else if (file_field.ends_with(".mp4") || file_field.ends_with(".webm") || file_field.ends_with(".avi")) {
            metadata.type = "video";
        } else if (file_field.ends_with(".html") || file_field.ends_with(".htm")) {
            metadata.type = "web";
        } else {
            metadata.type = "scene";  // Default to scene
        }
    } else if (metadata.type.empty()) {
        metadata.type = "scene";  // Fallback default
    }

    if (j.contains("description")) {
        metadata.description = j["description"].get<std::string>();
    }

    // Extract Steam Workshop ID from directory name or workshopid field
    if (j.contains("workshopid")) {
        metadata.steam_id = j["workshopid"].get<std::string>();
    } else {
        // Try to extract from directory name (e.g., 2667198601)
        std::string dirname = projectJsonPath.parent_path().filename().string();
        try {
            std::stoll(dirname); // If it's a number, it's likely a workshop ID
            metadata.steam_id = dirname;
        } catch (...) {}
    }

    // Extract preview image
    if (j.contains("preview")) {
        metadata.preview_path = (projectJsonPath.parent_path() / j["preview"].get<std::string>()).string();
    }

    // Extract resolution if available
    if (j.contains("general")) {
        auto& general = j["general"];
        if (general.contains("properties")) {
            // Some wallpapers store resolution in properties
        }
    }

    // Extract tags
    if (j.contains("tags")) {
        for (const auto& tag : j["tags"]) {
            if (tag.is_string()) {
                metadata.tags.push_back(tag.get<std::string>());
            }
        }
    }

    return metadata;
}

int LibraryManager::cleanOrphans() {
    auto wallpapers = m_db.getAllWallpapers();
    int removed = 0;

    for (const auto& wp : wallpapers) {
        if (!std::filesystem::exists(wp.path)) {
            m_db.removeWallpaper(wp.id);
            removed++;
            std::cout << "[WallForge] Removed orphan: " << wp.name << " (" << wp.path << ")" << std::endl;
        }
    }

    return removed;
}

LibraryManager::Stats LibraryManager::getStats() {
    Stats stats;
    stats.total_wallpapers = m_db.getWallpaperCount();
    stats.scene_count = static_cast<int>(m_db.getWallpapersByType("scene").size());
    stats.video_count = static_cast<int>(m_db.getWallpapersByType("video").size());
    stats.web_count = static_cast<int>(m_db.getWallpapersByType("web").size());
    stats.playlist_count = m_db.getPlaylistCount();
    stats.tag_count = static_cast<int>(m_db.getAllTags().size());
    return stats;
}

bool LibraryManager::isWallpaperDirectory(const std::filesystem::path& dir) {
    return std::filesystem::exists(dir / "project.json");
}

std::filesystem::path LibraryManager::findPreviewImage(const std::filesystem::path& wpDir) {
    // Common preview image names
    static const std::vector<std::string> previewNames = {
        "preview.jpg", "preview.png", "preview.gif",
        "Preview.jpg", "Preview.png", "Preview.gif",
        "preview.webp", "thumb.jpg", "thumb.png"
    };

    for (const auto& name : previewNames) {
        auto path = wpDir / name;
        if (std::filesystem::exists(path)) {
            return path;
        }
    }
    return {};
}

int64_t LibraryManager::calculateDirectorySize(const std::filesystem::path& dir) {
    int64_t totalSize = 0;
    try {
        for (const auto& entry : std::filesystem::recursive_directory_iterator(dir)) {
            if (entry.is_regular_file()) {
                totalSize += entry.file_size();
            }
        }
    } catch (...) {}
    return totalSize;
}

} // namespace WallForge
