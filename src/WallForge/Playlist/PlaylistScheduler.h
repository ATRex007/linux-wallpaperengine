#pragma once

/**
 * WallForge - Dynamic Wallpaper Engine for Linux
 * Based on linux-wallpaperengine (GPLv3)
 *
 * PlaylistScheduler.h - Playlist scheduling and wallpaper rotation
 */

#include "../Library/LibraryDatabase.h"

#include <chrono>
#include <functional>
#include <random>
#include <vector>
#include <string>

namespace WallForge {

enum class TransitionType {
    None,
    Fade,
    CrossFade,
    Slide
};

struct PlaylistState {
    int64_t playlist_id = -1;
    int current_index = 0;
    bool is_playing = false;
    bool is_shuffle = false;
    std::vector<int> shuffle_order;
    std::chrono::steady_clock::time_point last_change;
    std::vector<int64_t> history;  // Recently played wallpaper IDs
};

/**
 * Manages wallpaper playlist scheduling, transitions, and triggers.
 */
class PlaylistScheduler {
public:
    using WallpaperChangeCallback = std::function<void(int64_t wallpaperId, TransitionType transition, int durationMs)>;

    explicit PlaylistScheduler(LibraryDatabase& db);
    ~PlaylistScheduler();

    // ---- Playback Control ----

    /**
     * Start playing a playlist
     */
    bool play(int64_t playlistId);

    /**
     * Stop playback
     */
    void stop();

    /**
     * Go to next wallpaper in playlist
     */
    void next();

    /**
     * Go to previous wallpaper in playlist
     */
    void previous();

    /**
     * Check if it's time to change wallpaper.
     * Call this periodically (e.g., every second).
     * Returns true if a wallpaper change was triggered.
     */
    bool tick();

    // ---- State ----

    PlaylistState getState() const { return m_state; }
    bool isPlaying() const { return m_state.is_playing; }

    /**
     * Get the ID of the currently displayed wallpaper
     */
    int64_t getCurrentWallpaperId() const;

    // ---- Settings ----

    void setShuffle(bool shuffle);
    void setTransition(TransitionType type, int durationMs = 1000);

    // ---- Callback ----

    /**
     * Set callback for wallpaper changes.
     * This is called when a new wallpaper should be displayed.
     */
    void setOnWallpaperChange(WallpaperChangeCallback callback);

private:
    LibraryDatabase& m_db;
    PlaylistState m_state;
    std::vector<PlaylistEntry> m_entries;
    TransitionType m_transitionType = TransitionType::CrossFade;
    int m_transitionDurationMs = 1000;
    WallpaperChangeCallback m_changeCallback;
    std::mt19937 m_rng;

    void generateShuffleOrder();
    int getEffectiveIndex() const;
    void triggerChange(int64_t wallpaperId);
};

} // namespace WallForge
