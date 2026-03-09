#pragma once

/**
 * WallForge - Dynamic Wallpaper Engine for Linux
 * Based on linux-wallpaperengine (GPLv3)
 *
 * LibraryManager.h - Wallpaper library management (scanning, importing, indexing)
 */

#include "LibraryDatabase.h"
#include "../Core/Config.h"

#include <filesystem>
#include <functional>

namespace WallForge {

/**
 * Manages the wallpaper library: scanning directories, importing wallpapers,
 * and providing search/filter functionality.
 */
class LibraryManager {
public:
    using ProgressCallback = std::function<void(const std::string& currentPath, int scanned, int total)>;

    explicit LibraryManager(LibraryDatabase& db);

    /**
     * Scan configured directories for wallpapers and add them to the library.
     * Reads project.json from each wallpaper directory to extract metadata.
     */
    int scanDirectories(const std::vector<std::string>& paths, ProgressCallback progress = nullptr);

    /**
     * Scan a single directory for wallpapers.
     */
    int scanDirectory(const std::filesystem::path& dirPath, ProgressCallback progress = nullptr);

    /**
     * Import a wallpaper from a local file/directory.
     */
    int64_t importFromPath(const std::filesystem::path& path);

    /**
     * Import wallpapers from Steam Workshop content directory.
     * Scans ~/.steam/steam/steamapps/workshop/content/431960/
     */
    int importFromSteam(const std::filesystem::path& steamContentDir = "");

    /**
     * Detect wallpaper type from project.json
     * @return "scene", "video", "web", or empty string if not detectable
     */
    static std::string detectWallpaperType(const std::filesystem::path& projectJsonPath);

    /**
     * Parse project.json and extract metadata
     */
    static WallpaperMetadata parseProjectJson(const std::filesystem::path& projectJsonPath);

    /**
     * Remove wallpapers whose source files no longer exist.
     */
    int cleanOrphans();

    /**
     * Get library statistics
     */
    struct Stats {
        int total_wallpapers = 0;
        int scene_count = 0;
        int video_count = 0;
        int web_count = 0;
        int playlist_count = 0;
        int tag_count = 0;
    };
    Stats getStats();

private:
    LibraryDatabase& m_db;

    bool isWallpaperDirectory(const std::filesystem::path& dir);
    std::filesystem::path findPreviewImage(const std::filesystem::path& wpDir);
    int64_t calculateDirectorySize(const std::filesystem::path& dir);
};

} // namespace WallForge
