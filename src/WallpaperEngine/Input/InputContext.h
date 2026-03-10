#pragma once

#include "MouseInput.h"

namespace WallpaperEngine::Render::Drivers {
class VideoDriver;
}

namespace WallpaperEngine::Input {
class InputContext {
public:
    explicit InputContext (MouseInput& mouseInput);

    /**
     * Updates input information
     */
    void update ();

    [[nodiscard]] const MouseInput& getMouseInput () const;

    /**
     * Replace the active mouse input driver (e.g. switch to socket input in desktop mode)
     */
    void setMouseInput (MouseInput& mouseInput);

private:
    MouseInput* m_mouse;
};
} // namespace WallpaperEngine::Input
