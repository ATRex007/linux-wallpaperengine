#include "GLFWWindowOutput.h"
#include "GLFWOutputViewport.h"
#include "WallpaperEngine/Logging/Log.h"
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <ranges>
#include <unistd.h>

using namespace WallpaperEngine::Render::Drivers::Output;

GLFWWindowOutput::GLFWWindowOutput (ApplicationContext& context, VideoDriver& driver) : Output (context, driver) {
    if (this->m_context.settings.render.mode != Application::ApplicationContext::NORMAL_WINDOW
	&& this->m_context.settings.render.mode != Application::ApplicationContext::EXPLICIT_WINDOW
	&& this->m_context.settings.render.mode != Application::ApplicationContext::DESKTOP_BACKGROUND) {
	sLog.exception ("Initializing window output when not in output mode, how did you get here?!");
    }

    // window should be visible
    driver.showWindow ();

    if (this->m_context.settings.render.mode == Application::ApplicationContext::DESKTOP_BACKGROUND) {
	// Wayland GNOME fallback: get monitor resolution via GLFW and resize the
	// window to it. The viewport name must match the screen name from
	// --screen-root so that RenderContext::render() can find the wallpaper.
	GLFWmonitor* primary = glfwGetPrimaryMonitor ();
	if (primary) {
	    const GLFWvidmode* mode = glfwGetVideoMode (primary);
	    if (mode) {
		this->m_fullWidth = mode->width;
		this->m_fullHeight = mode->height;
		driver.resizeWindow (glm::ivec2 { mode->width, mode->height });
	    }
	}
	// Fall back to framebuffer size if monitor query failed
	if (this->m_fullWidth == 0 || this->m_fullHeight == 0) {
	    this->m_fullWidth = driver.getFramebufferSize ().x;
	    this->m_fullHeight = driver.getFramebufferSize ().y;
	}

	// Register a viewport per requested screen so wallpapers are matched
	for (const auto& screenName : this->m_context.settings.general.screenBackgrounds | std::views::keys) {
	    this->m_viewports[screenName]
		= new GLFWOutputViewport { { 0, 0, this->m_fullWidth, this->m_fullHeight }, screenName };
	}
    } else if (this->m_context.settings.render.mode == Application::ApplicationContext::EXPLICIT_WINDOW) {
	this->m_fullWidth = this->m_context.settings.render.window.geometry.z;
	this->m_fullHeight = this->m_context.settings.render.window.geometry.w;
	this->repositionWindow ();
    } else {
	// take the size from the driver (default window size)
	this->m_fullWidth = this->m_driver.getFramebufferSize ().x;
	this->m_fullHeight = this->m_driver.getFramebufferSize ().y;
    }

    // DESKTOP_BACKGROUND already registered screen-named viewports above
    if (this->m_context.settings.render.mode != Application::ApplicationContext::DESKTOP_BACKGROUND) {
	// register the default viewport
	this->m_viewports["default"]
	    = new GLFWOutputViewport { { 0, 0, this->m_fullWidth, this->m_fullHeight }, "default" };
    }
}

void GLFWWindowOutput::repositionWindow () const {
    // reposition the window
    this->m_driver.resizeWindow (this->m_context.settings.render.window.geometry);
}

void GLFWWindowOutput::reset () {
    if (this->m_context.settings.render.mode == Application::ApplicationContext::EXPLICIT_WINDOW) {
	this->repositionWindow ();
    }
}

bool GLFWWindowOutput::renderVFlip () const { return true; }

bool GLFWWindowOutput::renderMultiple () const { return false; }

bool GLFWWindowOutput::haveImageBuffer () const { return false; }

void* GLFWWindowOutput::getImageBuffer () const { return nullptr; }

uint32_t GLFWWindowOutput::getImageBufferSize () const { return 0; }

void GLFWWindowOutput::updateRender () const {
    if (this->m_context.settings.render.mode != Application::ApplicationContext::NORMAL_WINDOW
	&& this->m_context.settings.render.mode != Application::ApplicationContext::DESKTOP_BACKGROUND) {
	return;
    }

    // take the size from the driver (default window size)
    this->m_fullWidth = this->m_driver.getFramebufferSize ().x;
    this->m_fullHeight = this->m_driver.getFramebufferSize ().y;

    // update all registered viewports
    for (auto& [name, viewport] : this->m_viewports) {
	viewport->viewport = { 0, 0, this->m_fullWidth, this->m_fullHeight };
    }
}