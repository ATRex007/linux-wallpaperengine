#pragma once

#include "WallpaperEngine/Input/MouseInput.h"

#include <atomic>
#include <glm/vec2.hpp>
#include <string>

namespace WallpaperEngine::Input::Drivers {
/**
 * Receives mouse input from an external process (e.g. GNOME Shell extension)
 * via a Unix Domain Datagram Socket.
 *
 * Protocol (text datagrams):
 *   "M <x> <y>\n"               — mouse move
 *   "C <button> <pressed> <x> <y>\n" — click (button: 0=left 1=right, pressed: 0/1)
 */
class SocketMouseInput final : public MouseInput {
  public:
    /**
     * @param screenWidth  monitor width in pixels (for Y-flip to OpenGL coords)
     * @param screenHeight monitor height in pixels
     */
    SocketMouseInput (int screenWidth, int screenHeight);
    ~SocketMouseInput () override;

    void update () override;
    [[nodiscard]] glm::dvec2 position () const override;
    [[nodiscard]] MouseClickStatus leftClick () const override;
    [[nodiscard]] MouseClickStatus rightClick () const override;

  private:
    int m_fd = -1;
    int m_screenWidth;
    int m_screenHeight;
    std::string m_socketPath;

    glm::dvec2 m_position = {};
    MouseClickStatus m_leftClick = Released;
    MouseClickStatus m_rightClick = Released;

    int m_debugFrameCount = 0;
    int m_debugEventTotal = 0;
};
} // namespace WallpaperEngine::Input::Drivers
