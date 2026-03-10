/**
 * WallForge - Dynamic Wallpaper Engine for Linux
 * Based on linux-wallpaperengine (GPLv3)
 *
 * WallForgeApp.cpp - Main application implementation
 */

#include "WallForgeApp.h"

#include <iostream>
#include <iomanip>
#include <fstream>
#include <cstring>
#include <filesystem>
#include <unistd.h>
#include <sys/wait.h>
#include <csignal>
#include <termios.h>
#include <nlohmann/json.hpp>

namespace WallForge {

WallForgeApp::WallForgeApp() = default;
WallForgeApp::~WallForgeApp() = default;

bool WallForgeApp::init() {
    // Load configuration
    auto& config = Config::getInstance();
    if (!config.load()) {
        std::cerr << "[WallForge] Warning: Could not load config, using defaults" << std::endl;
    }

    // Open database
    auto dbPath = config.get().library.database_path;
    if (dbPath.empty()) {
        dbPath = Config::getDefaultDatabasePath().string();
    }

    if (!m_db.open(dbPath)) {
        std::cerr << "[WallForge] Error: Could not open database at " << dbPath << std::endl;
        return false;
    }

    // Initialize schema
    // First try to find schema.sql relative to executable, then use embedded schema
    std::string schemaSQL = R"(
        CREATE TABLE IF NOT EXISTS wallpapers (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            steam_id TEXT UNIQUE,
            name TEXT NOT NULL,
            type TEXT NOT NULL CHECK(type IN ('scene', 'video', 'web')),
            path TEXT NOT NULL UNIQUE,
            preview_path TEXT,
            description TEXT DEFAULT '',
            author TEXT DEFAULT '',
            rating INTEGER DEFAULT 0 CHECK(rating BETWEEN 0 AND 5),
            resolution_width INTEGER DEFAULT 0,
            resolution_height INTEGER DEFAULT 0,
            file_size_bytes INTEGER DEFAULT 0,
            added_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
            last_used_at TIMESTAMP,
            use_count INTEGER DEFAULT 0,
            is_favorite BOOLEAN DEFAULT 0
        );
        CREATE TABLE IF NOT EXISTS tags (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            name TEXT NOT NULL UNIQUE
        );
        CREATE TABLE IF NOT EXISTS wallpaper_tags (
            wallpaper_id INTEGER NOT NULL,
            tag_id INTEGER NOT NULL,
            PRIMARY KEY (wallpaper_id, tag_id),
            FOREIGN KEY (wallpaper_id) REFERENCES wallpapers(id) ON DELETE CASCADE,
            FOREIGN KEY (tag_id) REFERENCES tags(id) ON DELETE CASCADE
        );
        CREATE TABLE IF NOT EXISTS playlists (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            name TEXT NOT NULL,
            description TEXT DEFAULT '',
            is_shuffle BOOLEAN DEFAULT 0,
            transition_type TEXT DEFAULT 'crossfade',
            transition_duration_ms INTEGER DEFAULT 1000,
            created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
            updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
        );
        CREATE TABLE IF NOT EXISTS playlist_wallpapers (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            playlist_id INTEGER NOT NULL,
            wallpaper_id INTEGER NOT NULL,
            position INTEGER NOT NULL,
            duration_seconds INTEGER DEFAULT 300,
            FOREIGN KEY (playlist_id) REFERENCES playlists(id) ON DELETE CASCADE,
            FOREIGN KEY (wallpaper_id) REFERENCES wallpapers(id) ON DELETE CASCADE
        );
        CREATE TABLE IF NOT EXISTS performance_profiles (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            name TEXT NOT NULL UNIQUE,
            max_fps INTEGER DEFAULT 60,
            effects_enabled BOOLEAN DEFAULT 1,
            audio_enabled BOOLEAN DEFAULT 1,
            is_builtin BOOLEAN DEFAULT 0
        );
        CREATE TABLE IF NOT EXISTS app_rules (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            app_name TEXT NOT NULL,
            condition TEXT NOT NULL,
            action TEXT NOT NULL,
            action_value TEXT,
            is_enabled BOOLEAN DEFAULT 1
        );
        CREATE TABLE IF NOT EXISTS settings (
            key TEXT PRIMARY KEY,
            value TEXT NOT NULL
        );
        INSERT OR IGNORE INTO performance_profiles (name, max_fps, effects_enabled, audio_enabled, is_builtin)
        VALUES ('high', 60, 1, 1, 1), ('balanced', 30, 1, 1, 1), ('low', 15, 0, 0, 1);
    )";

    if (!m_db.initSchemaFromString(schemaSQL)) {
        std::cerr << "[WallForge] Warning: Schema initialization failed" << std::endl;
    }

    // Initialize subsystems
    m_libraryManager = std::make_unique<LibraryManager>(m_db);
    m_scheduler = std::make_unique<PlaylistScheduler>(m_db);
    m_perfManager = std::make_unique<PerformanceManager>();
    m_perfManager->init(config.get().performance);

    std::cout << "[WallForge] Initialized successfully" << std::endl;
    return true;
}

int WallForgeApp::processCommand(int argc, char* argv[]) {
    if (argc < 2) {
        printUsage();
        return 0;
    }

    std::string cmd = argv[1];

    if (cmd == "library" || cmd == "lib") {
        return handleLibraryCommand(argc - 1, argv + 1);
    } else if (cmd == "playlist" || cmd == "pl") {
        return handlePlaylistCommand(argc - 1, argv + 1);
    } else if (cmd == "perf" || cmd == "performance") {
        return handlePerfCommand(argc - 1, argv + 1);
    } else if (cmd == "run") {
        return handleRunCommand(argc - 1, argv + 1);
    } else if (cmd == "--version" || cmd == "-v") {
        printVersion();
        return 0;
    } else if (cmd == "--help" || cmd == "-h") {
        printUsage();
        return 0;
    } else {
        // Fall through to original linux-wallpaperengine behavior
        // This allows backward compatibility
        std::cout << "[WallForge] Unknown command: " << cmd << std::endl;
        printUsage();
        return 1;
    }
}

int WallForgeApp::handleLibraryCommand(int argc, char* argv[]) {
    if (argc < 2) {
        std::cout << "Usage: wallforge library <subcommand>" << std::endl;
        std::cout << "  scan    - Scan configured directories for wallpapers" << std::endl;
        std::cout << "  list    - List all wallpapers" << std::endl;
        std::cout << "  search  - Search wallpapers by name/description" << std::endl;
        std::cout << "  import  - Import wallpaper from path" << std::endl;
        std::cout << "  steam   - Import from Steam Workshop" << std::endl;
        std::cout << "  clean   - Remove orphaned entries" << std::endl;
        std::cout << "  stats   - Show library statistics" << std::endl;
        std::cout << "  tag     - Add tag to wallpaper" << std::endl;
        return 0;
    }

    std::string subcmd = argv[1];

    if (subcmd == "scan") {
        auto& config = Config::getInstance().get();
        int count = m_libraryManager->scanDirectories(config.library.paths);
        std::cout << "Scan complete: " << count << " new wallpapers added" << std::endl;
        return 0;
    }

    if (subcmd == "list") {
        auto wallpapers = m_db.getAllWallpapers();
        if (wallpapers.empty()) {
            std::cout << "Library is empty. Run 'wallforge library scan' to add wallpapers." << std::endl;
            return 0;
        }
        std::cout << std::left << std::setw(6) << "ID"
                  << std::setw(8) << "Type"
                  << std::setw(40) << "Name"
                  << std::setw(6) << "Rating"
                  << "Path" << std::endl;
        std::cout << std::string(100, '-') << std::endl;
        for (const auto& wp : wallpapers) {
            std::cout << std::left << std::setw(6) << wp.id
                      << std::setw(8) << wp.type
                      << std::setw(40) << wp.name.substr(0, 38)
                      << std::setw(6) << wp.rating
                      << wp.path << std::endl;
        }
        std::cout << "\nTotal: " << wallpapers.size() << " wallpapers" << std::endl;
        return 0;
    }

    if (subcmd == "search" && argc >= 3) {
        std::string query = argv[2];
        auto results = m_db.searchWallpapers(query);
        std::cout << "Search results for '" << query << "': " << results.size() << " found" << std::endl;
        for (const auto& wp : results) {
            std::cout << "  [" << wp.id << "] " << wp.name << " (" << wp.type << ")" << std::endl;
        }
        return 0;
    }

    if (subcmd == "import" && argc >= 3) {
        int64_t id = m_libraryManager->importFromPath(argv[2]);
        if (id > 0) {
            std::cout << "Imported wallpaper with ID: " << id << std::endl;
        } else {
            std::cout << "Import failed" << std::endl;
            return 1;
        }
        return 0;
    }

    if (subcmd == "steam") {
        int count = m_libraryManager->importFromSteam();
        std::cout << "Imported " << count << " wallpapers from Steam Workshop" << std::endl;
        return 0;
    }

    if (subcmd == "clean") {
        int removed = m_libraryManager->cleanOrphans();
        std::cout << "Cleaned " << removed << " orphaned entries" << std::endl;
        return 0;
    }

    if (subcmd == "stats") {
        auto stats = m_libraryManager->getStats();
        std::cout << "=== WallForge Library Statistics ===" << std::endl;
        std::cout << "Total wallpapers: " << stats.total_wallpapers << std::endl;
        std::cout << "  Scene:  " << stats.scene_count << std::endl;
        std::cout << "  Video:  " << stats.video_count << std::endl;
        std::cout << "  Web:    " << stats.web_count << std::endl;
        std::cout << "Playlists: " << stats.playlist_count << std::endl;
        std::cout << "Tags:      " << stats.tag_count << std::endl;
        return 0;
    }

    if (subcmd == "tag" && argc >= 4) {
        int64_t wpId = std::stoll(argv[2]);
        std::string tag = argv[3];
        if (m_db.addTag(wpId, tag)) {
            std::cout << "Tag '" << tag << "' added to wallpaper " << wpId << std::endl;
        } else {
            std::cout << "Failed to add tag" << std::endl;
            return 1;
        }
        return 0;
    }

    std::cout << "Unknown library subcommand: " << subcmd << std::endl;
    return 1;
}

int WallForgeApp::handlePlaylistCommand(int argc, char* argv[]) {
    if (argc < 2) {
        std::cout << "Usage: wallforge playlist <subcommand>" << std::endl;
        std::cout << "  list              - List all playlists" << std::endl;
        std::cout << "  create <name>     - Create a new playlist" << std::endl;
        std::cout << "  add <pid> <wid>   - Add wallpaper to playlist" << std::endl;
        std::cout << "  play <pid>        - Start playing playlist" << std::endl;
        std::cout << "  stop              - Stop playlist" << std::endl;
        std::cout << "  next              - Next wallpaper" << std::endl;
        std::cout << "  prev              - Previous wallpaper" << std::endl;
        return 0;
    }

    std::string subcmd = argv[1];

    if (subcmd == "list") {
        auto playlists = m_db.getAllPlaylists();
        if (playlists.empty()) {
            std::cout << "No playlists. Use 'wallforge playlist create <name>' to create one." << std::endl;
            return 0;
        }
        for (const auto& pl : playlists) {
            auto entries = m_db.getPlaylistEntries(pl.id);
            std::cout << "[" << pl.id << "] " << pl.name
                      << " (" << entries.size() << " wallpapers"
                      << ", shuffle=" << (pl.is_shuffle ? "yes" : "no")
                      << ", transition=" << pl.transition_type << ")" << std::endl;
        }
        return 0;
    }

    if (subcmd == "create" && argc >= 3) {
        PlaylistInfo info;
        info.name = argv[2];
        int64_t id = m_db.createPlaylist(info);
        if (id > 0) {
            std::cout << "Playlist created: " << info.name << " (ID: " << id << ")" << std::endl;
        }
        return 0;
    }

    if (subcmd == "add" && argc >= 4) {
        int64_t playlistId = std::stoll(argv[2]);
        int64_t wallpaperId = std::stoll(argv[3]);
        int duration = (argc >= 5) ? std::stoi(argv[4]) : 300;
        auto entries = m_db.getPlaylistEntries(playlistId);
        int position = static_cast<int>(entries.size());
        if (m_db.addToPlaylist(playlistId, wallpaperId, position, duration)) {
            std::cout << "Wallpaper " << wallpaperId << " added to playlist " << playlistId << std::endl;
        }
        return 0;
    }

    if (subcmd == "play" && argc >= 3) {
        int64_t playlistId = std::stoll(argv[2]);

        // Parse optional display mode
        std::string displayMode = "desktop";
        std::string displayArg;
        for (int i = 3; i < argc; i++) {
            std::string opt = argv[i];
            if (opt == "--window" && i + 1 < argc) {
                displayMode = "window";
                displayArg = argv[++i];
            } else if (opt == "--screen-root" && i + 1 < argc) {
                displayMode = "screen-root";
                displayArg = argv[++i];
            } else if (opt == "--desktop") {
                displayMode = "desktop";
            }
        }

        // For desktop mode, we'll detect per-wallpaper resolution in the callback
        bool isDesktopMode = (displayMode == "desktop");
        if (isDesktopMode) {
            displayMode = "window";  // Use window mode under the hood
        }

        // Set up wallpaper change callback that launches the engine
        m_scheduler->setOnWallpaperChange([this, displayMode, displayArg, isDesktopMode](int64_t wpId, TransitionType, int) {
            auto wp = m_db.getWallpaper(wpId);
            if (wp.has_value()) {
                std::cout << "\n>> Now playing: " << wp->name << " (" << wp->type << ")" << std::endl;
                m_db.markUsed(wpId);

                // Stop current engine, launch new one
                stopEngine();

                auto selfPath = std::filesystem::read_symlink("/proc/self/exe").parent_path();
                auto enginePath = selfPath / "linux-wallpaperengine";
                auto steamRoot = Config::findSteamRoot();
                std::string assetsDir;
                if (!steamRoot.empty()) {
                    auto assetsPath = steamRoot / "steamapps" / "common" / "wallpaper_engine" / "assets";
                    if (std::filesystem::exists(assetsPath)) {
                        assetsDir = assetsPath.string();
                    }
                }
                auto perfState = m_perfManager->getState();
                auto profile = m_perfManager->getProfile(perfState.active_profile);

                std::vector<std::string> args;
                args.push_back(enginePath.string());
                if (!assetsDir.empty()) {
                    args.push_back("--assets-dir");
                    args.push_back(assetsDir);
                }
                args.push_back("--fps");
                args.push_back(std::to_string(profile.max_fps));
                if (displayMode == "screen-root") {
                    args.push_back("--screen-root");
                    args.push_back(displayArg);
                } else if (isDesktopMode) {
                    // Desktop mode: use wallpaper's native resolution
                    auto wpRes = detectWallpaperResolution(wp->path);
                    std::string geom = "0x0x" + std::to_string(wpRes.first) + "x" + std::to_string(wpRes.second);
                    args.push_back("--window");
                    args.push_back(geom);
                    std::cout << "[WallForge] Desktop mode: " << wpRes.first << "x" << wpRes.second << std::endl;
                } else {
                    args.push_back("--window");
                    args.push_back(displayArg);
                }
                if (!profile.audio_enabled) {
                    args.push_back("--silent");
                }
                args.push_back(wp->path);

                pid_t pid = fork();
                if (pid == 0) {
                    std::vector<char*> cargs;
                    for (auto& a : args) cargs.push_back(a.data());
                    cargs.push_back(nullptr);
                    execvp(cargs[0], cargs.data());
                    _exit(1);
                } else if (pid > 0) {
                    m_enginePid = pid;
                }
            }
        });

        if (!m_scheduler->play(playlistId)) {
            std::cerr << "Failed to start playlist " << playlistId << std::endl;
            return 1;
        }

        std::cout << "=== Playlist Mode ===" << std::endl;
        std::cout << "Display: " << displayMode << " " << displayArg << std::endl;
        std::cout << "Commands: [n]ext  [p]rev  [s]top  [q]uit" << std::endl;
        std::cout << "Wallpapers will rotate automatically based on playlist duration." << std::endl;

        // Set up signal handler for clean exit
        s_interrupted.store(false);
        struct sigaction sa;
        sa.sa_handler = [](int) { WallForgeApp::s_interrupted.store(true); };
        sigemptyset(&sa.sa_mask);
        sa.sa_flags = 0;
        sigaction(SIGINT, &sa, nullptr);
        sigaction(SIGTERM, &sa, nullptr);

        // Interactive event loop
        // Set stdin to non-blocking for keyboard input
        struct termios oldt, newt;
        tcgetattr(STDIN_FILENO, &oldt);
        newt = oldt;
        newt.c_lflag &= ~(ICANON | ECHO);
        newt.c_cc[VMIN] = 0;
        newt.c_cc[VTIME] = 1;  // 100ms timeout
        tcsetattr(STDIN_FILENO, TCSANOW, &newt);

        while (!s_interrupted.load()) {
            // Check scheduler timer
            m_scheduler->tick();

            // Check for keyboard input
            char ch;
            if (read(STDIN_FILENO, &ch, 1) == 1) {
                switch (ch) {
                    case 'n': case 'N':
                        std::cout << "\n[Next]" << std::endl;
                        m_scheduler->next();
                        break;
                    case 'p': case 'P':
                        std::cout << "\n[Previous]" << std::endl;
                        m_scheduler->previous();
                        break;
                    case 's': case 'S':
                    case 'q': case 'Q':
                        s_interrupted.store(true);
                        break;
                }
            }

            // Reap zombie child processes
            int status;
            waitpid(-1, &status, WNOHANG);

            usleep(100000); // 100ms sleep
        }

        // Restore terminal
        tcsetattr(STDIN_FILENO, TCSANOW, &oldt);

        // Clean up
        stopEngine();
        m_scheduler->stop();
        std::cout << "\nPlaylist stopped." << std::endl;
        return 0;
    }

    if (subcmd == "stop") {
        m_scheduler->stop();
        return 0;
    }

    if (subcmd == "next") {
        m_scheduler->next();
        return 0;
    }

    if (subcmd == "prev") {
        m_scheduler->previous();
        return 0;
    }

    std::cout << "Unknown playlist subcommand: " << subcmd << std::endl;
    return 1;
}

int WallForgeApp::handlePerfCommand(int argc, char* argv[]) {
    if (argc < 2) {
        std::cout << "Usage: wallforge perf <subcommand>" << std::endl;
        std::cout << "  status          - Show current performance state" << std::endl;
        std::cout << "  profile <name>  - Set performance profile" << std::endl;
        std::cout << "  profiles        - List available profiles" << std::endl;
        std::cout << "  battery         - Show battery status" << std::endl;
        return 0;
    }

    std::string subcmd = argv[1];

    if (subcmd == "status") {
        auto state = m_perfManager->getState();
        std::cout << "=== Performance Status ===" << std::endl;
        std::cout << "Profile:  " << state.active_profile << std::endl;
        std::cout << "FPS:      " << state.current_fps << std::endl;
        std::cout << "Effects:  " << (state.effects_enabled ? "ON" : "OFF") << std::endl;
        std::cout << "Audio:    " << (state.audio_enabled ? "ON" : "OFF") << std::endl;
        std::cout << "Paused:   " << (state.is_paused ? "YES (" + state.pause_reason + ")" : "NO") << std::endl;
        return 0;
    }

    if (subcmd == "profile" && argc >= 3) {
        if (m_perfManager->setProfile(argv[2])) {
            std::cout << "Profile set to: " << argv[2] << std::endl;
        } else {
            std::cout << "Unknown profile: " << argv[2] << std::endl;
            return 1;
        }
        return 0;
    }

    if (subcmd == "profiles") {
        auto names = m_perfManager->getProfileNames();
        auto state = m_perfManager->getState();
        for (const auto& name : names) {
            auto profile = m_perfManager->getProfile(name);
            std::cout << (name == state.active_profile ? "* " : "  ")
                      << name
                      << " (fps=" << profile.max_fps
                      << ", effects=" << (profile.effects_enabled ? "on" : "off")
                      << ", audio=" << (profile.audio_enabled ? "on" : "off") << ")"
                      << std::endl;
        }
        return 0;
    }

    if (subcmd == "battery") {
        auto& battery = m_perfManager->getBatteryDetector();
        if (!battery.hasBattery()) {
            std::cout << "No battery detected (desktop system)" << std::endl;
            return 0;
        }
        auto status = battery.getStatus();
        std::cout << "=== Battery Status ===" << std::endl;
        std::cout << "State:   " << (status.state == PowerState::AC ? "AC" :
                                      status.state == PowerState::Battery ? "Battery" : "Unknown") << std::endl;
        std::cout << "Level:   " << status.percentage << "%" << std::endl;
        std::cout << "Charging: " << (status.is_charging ? "Yes" : "No") << std::endl;
        if (status.time_to_empty_seconds > 0) {
            int hours = status.time_to_empty_seconds / 3600;
            int mins = (status.time_to_empty_seconds % 3600) / 60;
            std::cout << "Time remaining: " << hours << "h " << mins << "m" << std::endl;
        }
        return 0;
    }

    std::cout << "Unknown perf subcommand: " << subcmd << std::endl;
    return 1;
}

int WallForgeApp::handleRunCommand(int argc, char* argv[]) {
    if (argc < 2) {
        std::cout << R"(Usage: wallforge run <target> [options...]

Target:
  <id>          Wallpaper ID from library (e.g., wallforge run 2)
  <path>        Direct path to wallpaper directory

Options:
  --desktop                Set as desktop wallpaper (GNOME extension required)
  --window <XxYxWxH>       Run in a window (e.g., --window 0x0x800x600)
  --screen-root <name>     Set as desktop wallpaper (X11/wlroots only)
  --fps <n>                Override FPS limit (default: from perf profile)
  --silent                 Mute audio
  --volume <n>             Set volume (0-128)
  --scaling <mode>         Scaling mode (default/stretch/fit/fill)

Examples:
  wallforge run 2                              # Desktop wallpaper (default)
  wallforge run 2 --desktop                    # Desktop wallpaper (explicit)
  wallforge run 2 --window 0x0x800x600         # Specific window size
  wallforge run ~/wallpapers/my_wp             # Direct path
)" << std::endl;
        return 0;
    }

    std::string target = argv[1];
    std::string wallpaperPath;

    // Resolve target: ID or path
    try {
        int64_t wpId = std::stoll(target);
        auto wp = m_db.getWallpaper(wpId);
        if (wp.has_value()) {
            wallpaperPath = wp->path;
            std::cout << "[WallForge] Running: " << wp->name << " (" << wp->type << ")" << std::endl;
            // Update usage stats
            m_db.markUsed(wpId);
        } else {
            std::cerr << "[WallForge] Wallpaper ID " << wpId << " not found in library" << std::endl;
            return 1;
        }
    } catch (const std::exception&) {
        // Not a number, treat as path
        wallpaperPath = target;
        if (!std::filesystem::exists(wallpaperPath)) {
            std::cerr << "[WallForge] Path does not exist: " << wallpaperPath << std::endl;
            return 1;
        }
    }

    // Find linux-wallpaperengine binary (same directory as wallforge)
    auto selfPath = std::filesystem::read_symlink("/proc/self/exe").parent_path();
    auto enginePath = selfPath / "linux-wallpaperengine";
    if (!std::filesystem::exists(enginePath)) {
        std::cerr << "[WallForge] Engine binary not found at: " << enginePath << std::endl;
        return 1;
    }

    // Find assets directory
    auto& config = Config::getInstance();
    auto steamRoot = Config::findSteamRoot();
    std::string assetsDir;
    if (!steamRoot.empty()) {
        auto assetsPath = steamRoot / "steamapps" / "common" / "wallpaper_engine" / "assets";
        if (std::filesystem::exists(assetsPath)) {
            assetsDir = assetsPath.string();
        }
    }

    // Build command-line arguments
    std::vector<std::string> args;
    args.push_back(enginePath.string());

    if (!assetsDir.empty()) {
        args.push_back("--assets-dir");
        args.push_back(assetsDir);
    }

    // Apply FPS from current performance profile
    auto perfState = m_perfManager->getState();
    auto profile = m_perfManager->getProfile(perfState.active_profile);
    bool fpsOverridden = false;
    bool hasScreenRoot = false;
    bool hasWindow = false;

    // Parse additional options and pass them through
    bool hasDesktop = false;
    for (int i = 2; i < argc; i++) {
        std::string opt = argv[i];
        if (opt == "--fps" && i + 1 < argc) {
            fpsOverridden = true;
        }
        if (opt == "--screen-root" || opt == "-r") {
            hasScreenRoot = true;
        }
        if (opt == "--window" || opt == "-w") {
            hasWindow = true;
        }
        if (opt == "--desktop") {
            hasDesktop = true;
            continue; // Don't pass --desktop to engine
        }
        args.push_back(argv[i]);
    }

    // Apply performance profile FPS if not overridden
    if (!fpsOverridden) {
        args.push_back("--fps");
        args.push_back(std::to_string(profile.max_fps));
    }

    // If no display mode specified, default to desktop mode (fullscreen window + GNOME extension)
    if (!hasScreenRoot && !hasWindow) {
        hasDesktop = true;
    }

    if (hasDesktop) {
        // Desktop mode: use wallpaper's native resolution for the window
        // The GNOME Shell extension will scale (cover) the clone to fill the monitor
        auto wpRes = detectWallpaperResolution(wallpaperPath);
        auto monRes = detectPrimaryMonitorResolution();
        int winW = wpRes.first;
        int winH = wpRes.second;
        std::cout << "[WallForge] Desktop mode: wallpaper=" << winW << "x" << winH
                  << " monitor=" << monRes.first << "x" << monRes.second << std::endl;
        std::string geometry = "0x0x" + std::to_string(winW) + "x" + std::to_string(winH);
        args.push_back("--window");
        args.push_back(geometry);
    }

    // Add wallpaper path as the positional argument
    args.push_back(wallpaperPath);

    // Build command string for display
    std::string cmdStr;
    for (const auto& a : args) {
        if (!cmdStr.empty()) cmdStr += " ";
        // Quote argument if it contains spaces
        if (a.find(' ') != std::string::npos) {
            cmdStr += "\"" + a + "\"";
        } else {
            cmdStr += a;
        }
    }
    std::cout << "[WallForge] Executing: " << cmdStr << std::endl;

    // Execute using execvp (replaces current process)
    // Or fork+exec to keep wallforge running for playlist control
    pid_t pid = fork();
    if (pid == 0) {
        // Child process
        std::vector<char*> cargs;
        for (auto& a : args) {
            cargs.push_back(a.data());
        }
        cargs.push_back(nullptr);
        execvp(cargs[0], cargs.data());
        // If exec fails
        perror("[WallForge] Failed to execute engine");
        _exit(1);
    } else if (pid > 0) {
        // Parent process - wait for child
        std::cout << "[WallForge] Engine started (PID: " << pid << ")" << std::endl;
        std::cout << "[WallForge] Press Ctrl+C to stop" << std::endl;
        int status;
        waitpid(pid, &status, 0);
        if (WIFEXITED(status)) {
            return WEXITSTATUS(status);
        }
        return 1;
    } else {
        perror("[WallForge] fork failed");
        return 1;
    }
}

void WallForgeApp::printUsage() {
    std::cout << R"(
WallForge - Dynamic Wallpaper Engine for Linux
Based on linux-wallpaperengine (GPLv3)

Usage: wallforge <command> [options]

Commands:
  library   (lib)  - Manage wallpaper library
  playlist  (pl)   - Manage playlists and slideshows
  perf             - Performance profiles and monitoring
  run              - Run a wallpaper
  --version (-v)   - Show version
  --help    (-h)   - Show this help

Examples:
  wallforge library scan          # Scan for wallpapers
  wallforge library list          # List all wallpapers
  wallforge playlist create "My Favorites"
  wallforge playlist add 1 42     # Add wallpaper 42 to playlist 1
  wallforge perf profile balanced # Set performance profile
  wallforge perf battery          # Check battery status

For the original linux-wallpaperengine commands, pass them directly:
  wallforge run --screen-root eDP-1 --bg 2667198601
)" << std::endl;
}

void WallForgeApp::printVersion() {
    std::cout << "WallForge v0.1.0" << std::endl;
    std::cout << "Based on linux-wallpaperengine" << std::endl;
    std::cout << "License: GPLv3" << std::endl;
}

// ---- Engine process management ----

std::atomic<bool> WallForgeApp::s_interrupted{false};

void WallForgeApp::launchEngine(const std::string& wallpaperPath) {
    // Stop existing engine if running
    stopEngine();

    auto selfPath = std::filesystem::read_symlink("/proc/self/exe").parent_path();
    auto enginePath = selfPath / "linux-wallpaperengine";

    auto steamRoot = Config::findSteamRoot();
    std::string assetsDir;
    if (!steamRoot.empty()) {
        auto assetsPath = steamRoot / "steamapps" / "common" / "wallpaper_engine" / "assets";
        if (std::filesystem::exists(assetsPath)) {
            assetsDir = assetsPath.string();
        }
    }

    auto perfState = m_perfManager->getState();
    auto profile = m_perfManager->getProfile(perfState.active_profile);

    std::vector<std::string> args;
    args.push_back(enginePath.string());
    if (!assetsDir.empty()) {
        args.push_back("--assets-dir");
        args.push_back(assetsDir);
    }
    args.push_back("--fps");
    args.push_back(std::to_string(profile.max_fps));
    // Use desktop mode with wallpaper's native resolution
    auto wpRes = detectWallpaperResolution(wallpaperPath);
    args.push_back("--window");
    args.push_back("0x0x" + std::to_string(wpRes.first) + "x" + std::to_string(wpRes.second));
    if (!profile.audio_enabled) {
        args.push_back("--silent");
    }
    args.push_back(wallpaperPath);

    pid_t pid = fork();
    if (pid == 0) {
        // Child process
        std::vector<char*> cargs;
        for (auto& a : args) {
            cargs.push_back(a.data());
        }
        cargs.push_back(nullptr);
        execvp(cargs[0], cargs.data());
        perror("[WallForge] Failed to execute engine");
        _exit(1);
    } else if (pid > 0) {
        m_enginePid = pid;
        std::cout << "[WallForge] Engine launched (PID: " << pid << ")" << std::endl;
    } else {
        perror("[WallForge] fork failed");
    }
}

void WallForgeApp::stopEngine() {
    if (m_enginePid > 0) {
        kill(m_enginePid, SIGTERM);
        int status;
        waitpid(m_enginePid, &status, 0);
        std::cout << "[WallForge] Engine stopped (PID: " << m_enginePid << ")" << std::endl;
        m_enginePid = -1;
    }
}

std::pair<int, int> WallForgeApp::detectPrimaryMonitorResolution() {
    // Try multiple methods to detect monitor resolution

    // Method 1: Parse xrandr output (works on X11 and XWayland)
    FILE* pipe = popen("xrandr --current 2>/dev/null | grep ' connected primary' | grep -oP '\\d+x\\d+' | head -1", "r");
    if (pipe) {
        char buf[64] = {};
        if (fgets(buf, sizeof(buf), pipe)) {
            int w = 0, h = 0;
            if (sscanf(buf, "%dx%d", &w, &h) == 2 && w > 0 && h > 0) {
                pclose(pipe);
                return {w, h};
            }
        }
        pclose(pipe);
    }

    // Method 2: Parse gnome-randr or wlr-randr for Wayland
    pipe = popen("gnome-randr 2>/dev/null | grep -A1 'associated physical monitor' | grep -oP '\\d+x\\d+' | head -1", "r");
    if (pipe) {
        char buf[64] = {};
        if (fgets(buf, sizeof(buf), pipe)) {
            int w = 0, h = 0;
            if (sscanf(buf, "%dx%d", &w, &h) == 2 && w > 0 && h > 0) {
                pclose(pipe);
                return {w, h};
            }
        }
        pclose(pipe);
    }

    // Method 3: Use GNOME D-Bus
    pipe = popen("gdbus call --session --dest org.gnome.Mutter.DisplayConfig --object-path /org/gnome/Mutter/DisplayConfig --method org.gnome.Mutter.DisplayConfig.GetCurrentState 2>/dev/null | grep -oP '\\d+, \\d+, [0-9.]+, [0-9.]+, [0-9.]+,' | head -1", "r");
    if (pipe) {
        char buf[128] = {};
        if (fgets(buf, sizeof(buf), pipe)) {
            // Parse Mutter display config output for resolution
        }
        pclose(pipe);
    }

    // Fallback: common resolution
    std::cerr << "[WallForge] Could not detect monitor resolution, using 1920x1200" << std::endl;
    return {1920, 1200};
}

std::pair<int, int> WallForgeApp::detectWallpaperResolution(const std::string& wallpaperPath) {
    namespace fs = std::filesystem;

    // Try reading scene.json for scene wallpapers
    fs::path scenePath = fs::path(wallpaperPath) / "scene.json";
    if (fs::exists(scenePath)) {
        std::ifstream file(scenePath);
        if (file.is_open()) {
            try {
                auto data = nlohmann::json::parse(file);
                if (data.contains("general") && data["general"].contains("orthogonalprojection")) {
                    auto& orth = data["general"]["orthogonalprojection"];
                    int w = orth.value("width", 0);
                    int h = orth.value("height", 0);
                    if (w > 0 && h > 0) {
                        std::cout << "[WallForge] Wallpaper native resolution: " << w << "x" << h << std::endl;
                        return {w, h};
                    }
                }
            } catch (...) {
                // JSON parse error, fall through
            }
        }
    }

    // Try reading project.json for width/height hints
    fs::path projectPath = fs::path(wallpaperPath) / "project.json";
    if (fs::exists(projectPath)) {
        std::ifstream file(projectPath);
        if (file.is_open()) {
            try {
                auto data = nlohmann::json::parse(file);
                int w = data.value("width", 0);
                int h = data.value("height", 0);
                if (w > 0 && h > 0) {
                    return {w, h};
                }
            } catch (...) {
                // Fall through
            }
        }
    }

    // Default: 1920x1080 (most common wallpaper resolution)
    std::cout << "[WallForge] Using default wallpaper resolution: 1920x1080" << std::endl;
    return {1920, 1080};
}

} // namespace WallForge
