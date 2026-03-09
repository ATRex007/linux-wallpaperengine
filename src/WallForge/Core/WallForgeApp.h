#pragma once

/**
 * WallForge - Dynamic Wallpaper Engine for Linux
 * Based on linux-wallpaperengine (GPLv3)
 *
 * WallForgeApp.h - Main application wrapper that integrates WallForge features
 *                  with the linux-wallpaperengine core.
 */

#include "Config.h"
#include "../Library/LibraryDatabase.h"
#include "../Library/LibraryManager.h"
#include "../Playlist/PlaylistScheduler.h"
#include "../Performance/PerformanceManager.h"

#include <memory>
#include <string>

namespace WallForge {

/**
 * WallForgeApp is the top-level orchestrator that ties together:
 * - Config management
 * - Library (wallpaper database & scanning)
 * - Playlist scheduler
 * - Performance manager
 * - The linux-wallpaperengine rendering core
 */
class WallForgeApp {
public:
    WallForgeApp();
    ~WallForgeApp();

    /**
     * Initialize the application. Loads config, opens database, etc.
     * @return true if initialization succeeded
     */
    bool init();

    /**
     * Process a CLI command and arguments.
     * This is the main entry point for the CLI interface.
     *
     * Supported commands:
     *   library scan          - Scan directories for wallpapers
     *   library list          - List all wallpapers in library
     *   library search <q>    - Search wallpapers
     *   library import <path> - Import a wallpaper
     *   library steam         - Import from Steam Workshop
     *   library clean         - Remove orphaned entries
     *   library stats         - Show library statistics
     *   library tag <id> <t>  - Add tag to wallpaper
     *
     *   playlist list                   - List all playlists
     *   playlist create <name>          - Create a new playlist
     *   playlist add <pid> <wid> [dur]  - Add wallpaper to playlist
     *   playlist play <pid>             - Start playing a playlist
     *   playlist stop                   - Stop playlist
     *   playlist next                   - Next wallpaper
     *   playlist prev                   - Previous wallpaper
     *
     *   perf status             - Show performance state
     *   perf profile <name>     - Set performance profile
     *   perf profiles           - List available profiles
     *   perf battery            - Show battery status
     *
     *   run <wallpaper_id_or_path> [options...]
     *                          - Run a wallpaper (delegates to engine core)
     */
    int processCommand(int argc, char* argv[]);

    // Access to subsystems
    Config& getConfig() { return Config::getInstance(); }
    LibraryDatabase& getDatabase() { return m_db; }
    LibraryManager& getLibraryManager() { return *m_libraryManager; }
    PlaylistScheduler& getPlaylistScheduler() { return *m_scheduler; }
    PerformanceManager& getPerformanceManager() { return *m_perfManager; }

private:
    LibraryDatabase m_db;
    std::unique_ptr<LibraryManager> m_libraryManager;
    std::unique_ptr<PlaylistScheduler> m_scheduler;
    std::unique_ptr<PerformanceManager> m_perfManager;

    // CLI command handlers
    int handleLibraryCommand(int argc, char* argv[]);
    int handlePlaylistCommand(int argc, char* argv[]);
    int handlePerfCommand(int argc, char* argv[]);
    int handleRunCommand(int argc, char* argv[]);
    void printUsage();
    void printVersion();
};

} // namespace WallForge
