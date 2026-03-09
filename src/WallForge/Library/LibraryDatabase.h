#pragma once

/**
 * WallForge - Dynamic Wallpaper Engine for Linux
 * Based on linux-wallpaperengine (GPLv3)
 *
 * LibraryDatabase.h - SQLite database wrapper for wallpaper library
 */

#include <sqlite3.h>
#include <string>
#include <vector>
#include <optional>
#include <filesystem>
#include <functional>

namespace WallForge {

struct WallpaperMetadata {
    int64_t id = 0;
    std::string steam_id;
    std::string name;
    std::string type;          // "scene", "video", "web"
    std::string path;
    std::string preview_path;
    std::string description;
    std::string author;
    int rating = 0;
    int resolution_width = 0;
    int resolution_height = 0;
    int64_t file_size_bytes = 0;
    std::string added_at;
    std::string last_used_at;
    int use_count = 0;
    bool is_favorite = false;
    std::vector<std::string> tags;
};

struct PlaylistInfo {
    int64_t id = 0;
    std::string name;
    std::string description;
    bool is_shuffle = false;
    std::string transition_type = "crossfade";
    int transition_duration_ms = 1000;
    std::string created_at;
    std::string updated_at;
};

struct PlaylistEntry {
    int64_t wallpaper_id;
    int position;
    int duration_seconds;
};

class LibraryDatabase {
public:
    LibraryDatabase();
    ~LibraryDatabase();

    // Database lifecycle
    bool open(const std::filesystem::path& dbPath);
    void close();
    bool isOpen() const { return m_db != nullptr; }

    // Initialize schema from SQL file
    bool initSchema(const std::filesystem::path& schemaPath);
    bool initSchemaFromString(const std::string& sql);

    // ---- Wallpaper CRUD ----
    int64_t addWallpaper(const WallpaperMetadata& wp);
    bool updateWallpaper(const WallpaperMetadata& wp);
    bool removeWallpaper(int64_t id);
    std::optional<WallpaperMetadata> getWallpaper(int64_t id);
    std::optional<WallpaperMetadata> getWallpaperByPath(const std::string& path);
    std::vector<WallpaperMetadata> getAllWallpapers();
    std::vector<WallpaperMetadata> searchWallpapers(const std::string& query);
    std::vector<WallpaperMetadata> getWallpapersByType(const std::string& type);
    std::vector<WallpaperMetadata> getFavorites();

    // ---- Tag operations ----
    bool addTag(int64_t wallpaperId, const std::string& tag);
    bool removeTag(int64_t wallpaperId, const std::string& tag);
    std::vector<std::string> getTags(int64_t wallpaperId);
    std::vector<std::string> getAllTags();

    // ---- Playlist CRUD ----
    int64_t createPlaylist(const PlaylistInfo& playlist);
    bool updatePlaylist(const PlaylistInfo& playlist);
    bool deletePlaylist(int64_t id);
    std::optional<PlaylistInfo> getPlaylist(int64_t id);
    std::vector<PlaylistInfo> getAllPlaylists();

    // Playlist entries
    bool addToPlaylist(int64_t playlistId, int64_t wallpaperId, int position, int durationSeconds = 300);
    bool removeFromPlaylist(int64_t playlistId, int64_t wallpaperId);
    std::vector<PlaylistEntry> getPlaylistEntries(int64_t playlistId);

    // ---- Usage tracking ----
    bool markUsed(int64_t wallpaperId);
    bool setFavorite(int64_t wallpaperId, bool favorite);
    bool setRating(int64_t wallpaperId, int rating);

    // ---- Settings ----
    bool setSetting(const std::string& key, const std::string& value);
    std::optional<std::string> getSetting(const std::string& key);

    // ---- Stats ----
    int getWallpaperCount();
    int getPlaylistCount();

private:
    sqlite3* m_db = nullptr;

    bool exec(const std::string& sql);
    bool execWithCallback(const std::string& sql,
                          std::function<void(int, char**, char**)> callback);

    WallpaperMetadata rowToWallpaper(sqlite3_stmt* stmt);
    PlaylistInfo rowToPlaylist(sqlite3_stmt* stmt);
};

} // namespace WallForge
