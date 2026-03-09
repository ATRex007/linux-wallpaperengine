/**
 * WallForge - Dynamic Wallpaper Engine for Linux
 * Based on linux-wallpaperengine (GPLv3)
 *
 * LibraryDatabase.cpp - SQLite database implementation
 */

#include "LibraryDatabase.h"

#include <fstream>
#include <sstream>
#include <iostream>
#include <cstring>

namespace WallForge {

LibraryDatabase::LibraryDatabase() = default;

LibraryDatabase::~LibraryDatabase() {
    close();
}

bool LibraryDatabase::open(const std::filesystem::path& dbPath) {
    if (m_db) close();

    std::filesystem::create_directories(dbPath.parent_path());

    int rc = sqlite3_open(dbPath.string().c_str(), &m_db);
    if (rc != SQLITE_OK) {
        std::cerr << "[WallForge] Failed to open database: " << sqlite3_errmsg(m_db) << std::endl;
        m_db = nullptr;
        return false;
    }

    // Enable WAL mode for better concurrent access
    exec("PRAGMA journal_mode=WAL");
    exec("PRAGMA foreign_keys=ON");

    std::cout << "[WallForge] Database opened: " << dbPath << std::endl;
    return true;
}

void LibraryDatabase::close() {
    if (m_db) {
        sqlite3_close(m_db);
        m_db = nullptr;
    }
}

bool LibraryDatabase::initSchema(const std::filesystem::path& schemaPath) {
    std::ifstream file(schemaPath);
    if (!file.is_open()) {
        std::cerr << "[WallForge] Failed to open schema file: " << schemaPath << std::endl;
        return false;
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    return initSchemaFromString(buffer.str());
}

bool LibraryDatabase::initSchemaFromString(const std::string& sql) {
    return exec(sql);
}

bool LibraryDatabase::exec(const std::string& sql) {
    if (!m_db) return false;
    char* errMsg = nullptr;
    int rc = sqlite3_exec(m_db, sql.c_str(), nullptr, nullptr, &errMsg);
    if (rc != SQLITE_OK) {
        std::cerr << "[WallForge] SQL error: " << (errMsg ? errMsg : "unknown") << std::endl;
        sqlite3_free(errMsg);
        return false;
    }
    return true;
}

// ---- Wallpaper CRUD ----

int64_t LibraryDatabase::addWallpaper(const WallpaperMetadata& wp) {
    if (!m_db) return -1;

    const char* sql = R"(
        INSERT INTO wallpapers (steam_id, name, type, path, preview_path, description, author,
                                rating, resolution_width, resolution_height, file_size_bytes)
        VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
    )";

    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        std::cerr << "[WallForge] Failed to prepare statement: " << sqlite3_errmsg(m_db) << std::endl;
        return -1;
    }

    sqlite3_bind_text(stmt, 1, wp.steam_id.empty() ? nullptr : wp.steam_id.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, wp.name.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, wp.type.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 4, wp.path.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 5, wp.preview_path.empty() ? nullptr : wp.preview_path.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 6, wp.description.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 7, wp.author.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 8, wp.rating);
    sqlite3_bind_int(stmt, 9, wp.resolution_width);
    sqlite3_bind_int(stmt, 10, wp.resolution_height);
    sqlite3_bind_int64(stmt, 11, wp.file_size_bytes);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) {
        std::cerr << "[WallForge] Failed to insert wallpaper: " << sqlite3_errmsg(m_db) << std::endl;
        return -1;
    }

    int64_t id = sqlite3_last_insert_rowid(m_db);

    // Add tags
    for (const auto& tag : wp.tags) {
        addTag(id, tag);
    }

    return id;
}

bool LibraryDatabase::updateWallpaper(const WallpaperMetadata& wp) {
    if (!m_db) return false;

    const char* sql = R"(
        UPDATE wallpapers SET name=?, type=?, path=?, preview_path=?, description=?,
               author=?, rating=?, resolution_width=?, resolution_height=?, file_size_bytes=?,
               is_favorite=?
        WHERE id=?
    )";

    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;

    sqlite3_bind_text(stmt, 1, wp.name.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, wp.type.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, wp.path.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 4, wp.preview_path.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 5, wp.description.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 6, wp.author.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 7, wp.rating);
    sqlite3_bind_int(stmt, 8, wp.resolution_width);
    sqlite3_bind_int(stmt, 9, wp.resolution_height);
    sqlite3_bind_int64(stmt, 10, wp.file_size_bytes);
    sqlite3_bind_int(stmt, 11, wp.is_favorite ? 1 : 0);
    sqlite3_bind_int64(stmt, 12, wp.id);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return rc == SQLITE_DONE;
}

bool LibraryDatabase::removeWallpaper(int64_t id) {
    if (!m_db) return false;
    std::string sql = "DELETE FROM wallpapers WHERE id=" + std::to_string(id);
    return exec(sql);
}

WallpaperMetadata LibraryDatabase::rowToWallpaper(sqlite3_stmt* stmt) {
    WallpaperMetadata wp;
    wp.id = sqlite3_column_int64(stmt, 0);

    auto getText = [&](int col) -> std::string {
        const char* text = reinterpret_cast<const char*>(sqlite3_column_text(stmt, col));
        return text ? text : "";
    };

    wp.steam_id = getText(1);
    wp.name = getText(2);
    wp.type = getText(3);
    wp.path = getText(4);
    wp.preview_path = getText(5);
    wp.description = getText(6);
    wp.author = getText(7);
    wp.rating = sqlite3_column_int(stmt, 8);
    wp.resolution_width = sqlite3_column_int(stmt, 9);
    wp.resolution_height = sqlite3_column_int(stmt, 10);
    wp.file_size_bytes = sqlite3_column_int64(stmt, 11);
    wp.added_at = getText(12);
    wp.last_used_at = getText(13);
    wp.use_count = sqlite3_column_int(stmt, 14);
    wp.is_favorite = sqlite3_column_int(stmt, 15) != 0;

    // Load tags
    wp.tags = getTags(wp.id);

    return wp;
}

std::optional<WallpaperMetadata> LibraryDatabase::getWallpaper(int64_t id) {
    if (!m_db) return std::nullopt;

    const char* sql = "SELECT * FROM wallpapers WHERE id=?";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) return std::nullopt;

    sqlite3_bind_int64(stmt, 1, id);
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        auto wp = rowToWallpaper(stmt);
        sqlite3_finalize(stmt);
        return wp;
    }
    sqlite3_finalize(stmt);
    return std::nullopt;
}

std::optional<WallpaperMetadata> LibraryDatabase::getWallpaperByPath(const std::string& path) {
    if (!m_db) return std::nullopt;

    const char* sql = "SELECT * FROM wallpapers WHERE path=?";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) return std::nullopt;

    sqlite3_bind_text(stmt, 1, path.c_str(), -1, SQLITE_TRANSIENT);
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        auto wp = rowToWallpaper(stmt);
        sqlite3_finalize(stmt);
        return wp;
    }
    sqlite3_finalize(stmt);
    return std::nullopt;
}

std::vector<WallpaperMetadata> LibraryDatabase::getAllWallpapers() {
    std::vector<WallpaperMetadata> results;
    if (!m_db) return results;

    const char* sql = "SELECT * FROM wallpapers ORDER BY name";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) return results;

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        results.push_back(rowToWallpaper(stmt));
    }
    sqlite3_finalize(stmt);
    return results;
}

std::vector<WallpaperMetadata> LibraryDatabase::searchWallpapers(const std::string& query) {
    std::vector<WallpaperMetadata> results;
    if (!m_db) return results;

    const char* sql = R"(
        SELECT * FROM wallpapers
        WHERE name LIKE ? OR description LIKE ? OR author LIKE ?
        ORDER BY name
    )";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) return results;

    std::string pattern = "%" + query + "%";
    sqlite3_bind_text(stmt, 1, pattern.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, pattern.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, pattern.c_str(), -1, SQLITE_TRANSIENT);

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        results.push_back(rowToWallpaper(stmt));
    }
    sqlite3_finalize(stmt);
    return results;
}

std::vector<WallpaperMetadata> LibraryDatabase::getWallpapersByType(const std::string& type) {
    std::vector<WallpaperMetadata> results;
    if (!m_db) return results;

    const char* sql = "SELECT * FROM wallpapers WHERE type=? ORDER BY name";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) return results;

    sqlite3_bind_text(stmt, 1, type.c_str(), -1, SQLITE_TRANSIENT);
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        results.push_back(rowToWallpaper(stmt));
    }
    sqlite3_finalize(stmt);
    return results;
}

std::vector<WallpaperMetadata> LibraryDatabase::getFavorites() {
    std::vector<WallpaperMetadata> results;
    if (!m_db) return results;

    const char* sql = "SELECT * FROM wallpapers WHERE is_favorite=1 ORDER BY name";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) return results;

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        results.push_back(rowToWallpaper(stmt));
    }
    sqlite3_finalize(stmt);
    return results;
}

// ---- Tags ----

bool LibraryDatabase::addTag(int64_t wallpaperId, const std::string& tag) {
    if (!m_db) return false;

    // Insert tag if not exists
    exec("INSERT OR IGNORE INTO tags (name) VALUES ('" + tag + "')");

    // Get tag ID
    const char* sql = "SELECT id FROM tags WHERE name=?";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;

    sqlite3_bind_text(stmt, 1, tag.c_str(), -1, SQLITE_TRANSIENT);
    int64_t tagId = -1;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        tagId = sqlite3_column_int64(stmt, 0);
    }
    sqlite3_finalize(stmt);

    if (tagId < 0) return false;

    // Link wallpaper to tag
    std::string linkSql = "INSERT OR IGNORE INTO wallpaper_tags (wallpaper_id, tag_id) VALUES ("
                         + std::to_string(wallpaperId) + "," + std::to_string(tagId) + ")";
    return exec(linkSql);
}

bool LibraryDatabase::removeTag(int64_t wallpaperId, const std::string& tag) {
    if (!m_db) return false;

    const char* sql = R"(
        DELETE FROM wallpaper_tags WHERE wallpaper_id=? AND tag_id=(SELECT id FROM tags WHERE name=?)
    )";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;

    sqlite3_bind_int64(stmt, 1, wallpaperId);
    sqlite3_bind_text(stmt, 2, tag.c_str(), -1, SQLITE_TRANSIENT);
    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return rc == SQLITE_DONE;
}

std::vector<std::string> LibraryDatabase::getTags(int64_t wallpaperId) {
    std::vector<std::string> tags;
    if (!m_db) return tags;

    const char* sql = R"(
        SELECT t.name FROM tags t
        JOIN wallpaper_tags wt ON wt.tag_id = t.id
        WHERE wt.wallpaper_id=?
        ORDER BY t.name
    )";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) return tags;

    sqlite3_bind_int64(stmt, 1, wallpaperId);
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        const char* name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        if (name) tags.emplace_back(name);
    }
    sqlite3_finalize(stmt);
    return tags;
}

std::vector<std::string> LibraryDatabase::getAllTags() {
    std::vector<std::string> tags;
    if (!m_db) return tags;

    const char* sql = "SELECT name FROM tags ORDER BY name";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) return tags;

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        const char* name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        if (name) tags.emplace_back(name);
    }
    sqlite3_finalize(stmt);
    return tags;
}

// ---- Playlists ----

int64_t LibraryDatabase::createPlaylist(const PlaylistInfo& playlist) {
    if (!m_db) return -1;

    const char* sql = R"(
        INSERT INTO playlists (name, description, is_shuffle, transition_type, transition_duration_ms)
        VALUES (?, ?, ?, ?, ?)
    )";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) return -1;

    sqlite3_bind_text(stmt, 1, playlist.name.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, playlist.description.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 3, playlist.is_shuffle ? 1 : 0);
    sqlite3_bind_text(stmt, 4, playlist.transition_type.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 5, playlist.transition_duration_ms);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return rc == SQLITE_DONE ? sqlite3_last_insert_rowid(m_db) : -1;
}

bool LibraryDatabase::updatePlaylist(const PlaylistInfo& playlist) {
    if (!m_db) return false;

    const char* sql = R"(
        UPDATE playlists SET name=?, description=?, is_shuffle=?, transition_type=?,
               transition_duration_ms=?, updated_at=CURRENT_TIMESTAMP
        WHERE id=?
    )";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;

    sqlite3_bind_text(stmt, 1, playlist.name.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, playlist.description.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 3, playlist.is_shuffle ? 1 : 0);
    sqlite3_bind_text(stmt, 4, playlist.transition_type.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 5, playlist.transition_duration_ms);
    sqlite3_bind_int64(stmt, 6, playlist.id);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return rc == SQLITE_DONE;
}

bool LibraryDatabase::deletePlaylist(int64_t id) {
    return exec("DELETE FROM playlists WHERE id=" + std::to_string(id));
}

PlaylistInfo LibraryDatabase::rowToPlaylist(sqlite3_stmt* stmt) {
    PlaylistInfo p;
    p.id = sqlite3_column_int64(stmt, 0);
    auto getText = [&](int col) -> std::string {
        const char* text = reinterpret_cast<const char*>(sqlite3_column_text(stmt, col));
        return text ? text : "";
    };
    p.name = getText(1);
    p.description = getText(2);
    p.is_shuffle = sqlite3_column_int(stmt, 3) != 0;
    p.transition_type = getText(4);
    p.transition_duration_ms = sqlite3_column_int(stmt, 5);
    p.created_at = getText(6);
    p.updated_at = getText(7);
    return p;
}

std::optional<PlaylistInfo> LibraryDatabase::getPlaylist(int64_t id) {
    if (!m_db) return std::nullopt;

    const char* sql = "SELECT * FROM playlists WHERE id=?";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) return std::nullopt;

    sqlite3_bind_int64(stmt, 1, id);
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        auto p = rowToPlaylist(stmt);
        sqlite3_finalize(stmt);
        return p;
    }
    sqlite3_finalize(stmt);
    return std::nullopt;
}

std::vector<PlaylistInfo> LibraryDatabase::getAllPlaylists() {
    std::vector<PlaylistInfo> results;
    if (!m_db) return results;

    const char* sql = "SELECT * FROM playlists ORDER BY name";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) return results;

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        results.push_back(rowToPlaylist(stmt));
    }
    sqlite3_finalize(stmt);
    return results;
}

bool LibraryDatabase::addToPlaylist(int64_t playlistId, int64_t wallpaperId, int position, int durationSeconds) {
    if (!m_db) return false;

    const char* sql = R"(
        INSERT INTO playlist_wallpapers (playlist_id, wallpaper_id, position, duration_seconds)
        VALUES (?, ?, ?, ?)
    )";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;

    sqlite3_bind_int64(stmt, 1, playlistId);
    sqlite3_bind_int64(stmt, 2, wallpaperId);
    sqlite3_bind_int(stmt, 3, position);
    sqlite3_bind_int(stmt, 4, durationSeconds);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return rc == SQLITE_DONE;
}

bool LibraryDatabase::removeFromPlaylist(int64_t playlistId, int64_t wallpaperId) {
    if (!m_db) return false;
    std::string sql = "DELETE FROM playlist_wallpapers WHERE playlist_id=" +
                      std::to_string(playlistId) + " AND wallpaper_id=" + std::to_string(wallpaperId);
    return exec(sql);
}

std::vector<PlaylistEntry> LibraryDatabase::getPlaylistEntries(int64_t playlistId) {
    std::vector<PlaylistEntry> entries;
    if (!m_db) return entries;

    const char* sql = "SELECT wallpaper_id, position, duration_seconds FROM playlist_wallpapers WHERE playlist_id=? ORDER BY position";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) return entries;

    sqlite3_bind_int64(stmt, 1, playlistId);
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        PlaylistEntry e;
        e.wallpaper_id = sqlite3_column_int64(stmt, 0);
        e.position = sqlite3_column_int(stmt, 1);
        e.duration_seconds = sqlite3_column_int(stmt, 2);
        entries.push_back(e);
    }
    sqlite3_finalize(stmt);
    return entries;
}

// ---- Usage tracking ----

bool LibraryDatabase::markUsed(int64_t wallpaperId) {
    std::string sql = "UPDATE wallpapers SET last_used_at=CURRENT_TIMESTAMP, use_count=use_count+1 WHERE id=" +
                      std::to_string(wallpaperId);
    return exec(sql);
}

bool LibraryDatabase::setFavorite(int64_t wallpaperId, bool favorite) {
    std::string sql = "UPDATE wallpapers SET is_favorite=" + std::to_string(favorite ? 1 : 0) +
                      " WHERE id=" + std::to_string(wallpaperId);
    return exec(sql);
}

bool LibraryDatabase::setRating(int64_t wallpaperId, int rating) {
    if (rating < 0 || rating > 5) return false;
    std::string sql = "UPDATE wallpapers SET rating=" + std::to_string(rating) +
                      " WHERE id=" + std::to_string(wallpaperId);
    return exec(sql);
}

// ---- Settings ----

bool LibraryDatabase::setSetting(const std::string& key, const std::string& value) {
    if (!m_db) return false;

    const char* sql = "INSERT OR REPLACE INTO settings (key, value) VALUES (?, ?)";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;

    sqlite3_bind_text(stmt, 1, key.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, value.c_str(), -1, SQLITE_TRANSIENT);
    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return rc == SQLITE_DONE;
}

std::optional<std::string> LibraryDatabase::getSetting(const std::string& key) {
    if (!m_db) return std::nullopt;

    const char* sql = "SELECT value FROM settings WHERE key=?";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) return std::nullopt;

    sqlite3_bind_text(stmt, 1, key.c_str(), -1, SQLITE_TRANSIENT);
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        const char* val = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        std::string result = val ? val : "";
        sqlite3_finalize(stmt);
        return result;
    }
    sqlite3_finalize(stmt);
    return std::nullopt;
}

// ---- Stats ----

int LibraryDatabase::getWallpaperCount() {
    if (!m_db) return 0;
    const char* sql = "SELECT COUNT(*) FROM wallpapers";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) return 0;
    int count = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        count = sqlite3_column_int(stmt, 0);
    }
    sqlite3_finalize(stmt);
    return count;
}

int LibraryDatabase::getPlaylistCount() {
    if (!m_db) return 0;
    const char* sql = "SELECT COUNT(*) FROM playlists";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) return 0;
    int count = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        count = sqlite3_column_int(stmt, 0);
    }
    sqlite3_finalize(stmt);
    return count;
}

bool LibraryDatabase::execWithCallback(const std::string& sql,
                                        std::function<void(int, char**, char**)> callback) {
    // Not used currently, but available for future use
    return exec(sql);
}

} // namespace WallForge
