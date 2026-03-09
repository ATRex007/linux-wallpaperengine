/**
 * WallForge - Dynamic Wallpaper Engine for Linux
 * Based on linux-wallpaperengine (GPLv3)
 *
 * PlaylistScheduler.cpp - Playlist scheduling implementation
 */

#include "PlaylistScheduler.h"

#include <iostream>
#include <algorithm>
#include <numeric>

namespace WallForge {

PlaylistScheduler::PlaylistScheduler(LibraryDatabase& db)
    : m_db(db)
    , m_rng(std::random_device{}())
{
}

PlaylistScheduler::~PlaylistScheduler() {
    stop();
}

bool PlaylistScheduler::play(int64_t playlistId) {
    auto playlist = m_db.getPlaylist(playlistId);
    if (!playlist.has_value()) {
        std::cerr << "[WallForge] Playlist not found: " << playlistId << std::endl;
        return false;
    }

    m_entries = m_db.getPlaylistEntries(playlistId);
    if (m_entries.empty()) {
        std::cerr << "[WallForge] Playlist is empty: " << playlist->name << std::endl;
        return false;
    }

    m_state.playlist_id = playlistId;
    m_state.current_index = 0;
    m_state.is_playing = true;
    m_state.is_shuffle = playlist->is_shuffle;
    m_state.last_change = std::chrono::steady_clock::now();

    // Parse transition settings
    if (playlist->transition_type == "fade") m_transitionType = TransitionType::Fade;
    else if (playlist->transition_type == "crossfade") m_transitionType = TransitionType::CrossFade;
    else if (playlist->transition_type == "slide") m_transitionType = TransitionType::Slide;
    else m_transitionType = TransitionType::None;
    m_transitionDurationMs = playlist->transition_duration_ms;

    if (m_state.is_shuffle) {
        generateShuffleOrder();
    }

    // Trigger first wallpaper
    triggerChange(getCurrentWallpaperId());

    std::cout << "[WallForge] Playlist started: " << playlist->name
              << " (" << m_entries.size() << " wallpapers)" << std::endl;
    return true;
}

void PlaylistScheduler::stop() {
    m_state.is_playing = false;
    m_state.playlist_id = -1;
    m_entries.clear();
    std::cout << "[WallForge] Playlist stopped" << std::endl;
}

void PlaylistScheduler::next() {
    if (!m_state.is_playing || m_entries.empty()) return;

    m_state.current_index++;
    if (m_state.current_index >= static_cast<int>(m_entries.size())) {
        m_state.current_index = 0;
        if (m_state.is_shuffle) {
            generateShuffleOrder();
        }
    }

    m_state.last_change = std::chrono::steady_clock::now();
    triggerChange(getCurrentWallpaperId());
}

void PlaylistScheduler::previous() {
    if (!m_state.is_playing || m_entries.empty()) return;

    m_state.current_index--;
    if (m_state.current_index < 0) {
        m_state.current_index = static_cast<int>(m_entries.size()) - 1;
    }

    m_state.last_change = std::chrono::steady_clock::now();
    triggerChange(getCurrentWallpaperId());
}

bool PlaylistScheduler::tick() {
    if (!m_state.is_playing || m_entries.empty()) return false;

    int effectiveIdx = getEffectiveIndex();
    if (effectiveIdx < 0 || effectiveIdx >= static_cast<int>(m_entries.size())) return false;

    int durationSeconds = m_entries[effectiveIdx].duration_seconds;
    auto elapsed = std::chrono::steady_clock::now() - m_state.last_change;
    auto elapsedSeconds = std::chrono::duration_cast<std::chrono::seconds>(elapsed).count();

    if (elapsedSeconds >= durationSeconds) {
        next();
        return true;
    }

    return false;
}

int64_t PlaylistScheduler::getCurrentWallpaperId() const {
    if (m_entries.empty()) return -1;

    int effectiveIdx = getEffectiveIndex();
    if (effectiveIdx < 0 || effectiveIdx >= static_cast<int>(m_entries.size())) return -1;

    return m_entries[effectiveIdx].wallpaper_id;
}

void PlaylistScheduler::setShuffle(bool shuffle) {
    m_state.is_shuffle = shuffle;
    if (shuffle) {
        generateShuffleOrder();
    }
}

void PlaylistScheduler::setTransition(TransitionType type, int durationMs) {
    m_transitionType = type;
    m_transitionDurationMs = durationMs;
}

void PlaylistScheduler::setOnWallpaperChange(WallpaperChangeCallback callback) {
    m_changeCallback = std::move(callback);
}

void PlaylistScheduler::generateShuffleOrder() {
    m_state.shuffle_order.resize(m_entries.size());
    std::iota(m_state.shuffle_order.begin(), m_state.shuffle_order.end(), 0);
    std::shuffle(m_state.shuffle_order.begin(), m_state.shuffle_order.end(), m_rng);
}

int PlaylistScheduler::getEffectiveIndex() const {
    if (m_state.is_shuffle && !m_state.shuffle_order.empty()) {
        int idx = m_state.current_index % static_cast<int>(m_state.shuffle_order.size());
        return m_state.shuffle_order[idx];
    }
    return m_state.current_index;
}

void PlaylistScheduler::triggerChange(int64_t wallpaperId) {
    if (wallpaperId < 0) return;

    // Track in history
    m_state.history.push_back(wallpaperId);
    if (m_state.history.size() > 50) {
        m_state.history.erase(m_state.history.begin());
    }

    // Mark as used in database
    m_db.markUsed(wallpaperId);

    if (m_changeCallback) {
        m_changeCallback(wallpaperId, m_transitionType, m_transitionDurationMs);
    }

    auto wp = m_db.getWallpaper(wallpaperId);
    if (wp.has_value()) {
        std::cout << "[WallForge] Now showing: " << wp->name << std::endl;
    }
}

} // namespace WallForge
