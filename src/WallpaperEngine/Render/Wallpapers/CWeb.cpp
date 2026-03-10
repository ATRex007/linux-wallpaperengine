// This code is a modification of the original projects that can be found at
// https://github.com/if1live/cef-gl-example
// https://github.com/andmcgregor/cefgui
#include "CWeb.h"
#include "WallpaperEngine/WebBrowser/CEF/WPSchemeHandlerFactory.h"

#include "WallpaperEngine/Data/Model/Project.h"
#include "WallpaperEngine/Data/Model/Wallpaper.h"

#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <sstream>

using namespace WallpaperEngine::Render;
using namespace WallpaperEngine::Render::Wallpapers;

using namespace WallpaperEngine::WebBrowser;
using namespace WallpaperEngine::WebBrowser::CEF;

CWeb::CWeb (
    const Wallpaper& wallpaper, RenderContext& context, AudioContext& audioContext, WebBrowserContext& browserContext,
    const WallpaperState::TextureUVsScaling& scalingMode, const uint32_t& clampMode
) : CWallpaper (wallpaper, context, audioContext, scalingMode, clampMode), m_browserContext (browserContext) {
    // Web wallpapers are rendered at 16:9; use ZoomFill to crop-and-fill
    // non-16:9 viewports so there are no blank bars.
    this->m_state.setTextureUVsStrategy (WallpaperState::TextureUVsScaling::ZoomFillUVs);

    // setup framebuffers
    this->setupFramebuffers ();

    CefWindowInfo window_info;
    window_info.SetAsWindowless (0);

    this->m_renderHandler = new WebBrowser::CEF::RenderHandler (this);

    CefBrowserSettings browserSettings;
    // documentaion says that 60 fps is maximum value
    browserSettings.windowless_frame_rate = std::max (60, context.getApp ().getContext ().settings.render.maximumFPS);

    this->m_client = new WebBrowser::CEF::BrowserClient (m_renderHandler);
    // use the custom scheme for the wallpaper's files
    const std::string htmlURL = WPSchemeHandlerFactory::generateSchemeName (this->getWeb ().project.workshopId)
	+ "://root/" + this->getWeb ().filename;
    this->m_browser
	= CefBrowserHost::CreateBrowserSync (window_info, this->m_client, htmlURL, browserSettings, nullptr, nullptr);

    // Grant initial focus so the browser processes user interaction events
    this->m_browser->GetHost ()->SetFocus (true);

    this->m_panelPaddingLeft = detectPanelPaddingLeft ();
}

void CWeb::setSize (const int width, const int height) {
    this->m_width = width > 0 ? width : this->m_width;
    this->m_height = height > 0 ? height : this->m_height;

    // do not refresh the texture if any of the sizes are invalid
    if (this->m_width <= 0 || this->m_height <= 0) {
	return;
    }

    // reconfigure the texture
    glBindTexture (GL_TEXTURE_2D, this->getWallpaperTexture ());
    glTexImage2D (
	GL_TEXTURE_2D, 0, GL_RGBA8, this->getWidth (), this->getHeight (), 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr
    );

    // Notify cef that it was resized(maybe it's not even needed)
    this->m_browser->GetHost ()->WasResized ();
}

void CWeb::renderFrame (const glm::ivec4& viewport) {
    // Render CEF at the viewport width but with 16:9 aspect ratio.
    // Web wallpapers are typically designed for 16:9; rendering at a non-16:9
    // viewport (e.g. 16:10) would create blank space in the HTML layout.
    // The ZoomFill UV scaling in CWallpaper::render then crops to fill the screen.
    const int cefWidth = viewport.z;
    const int cefHeight = (viewport.z * 9) / 16;

    if (cefWidth != this->getWidth () || cefHeight != this->getHeight ()) {
	this->setSize (cefWidth, cefHeight);
    }

    // ensure the virtual mouse position is up to date
    this->updateMouse (viewport);

    // Inject CSS for panel offset once DOM is available
    if (this->m_panelPaddingLeft > 0 && !this->m_cssInjected) {
	auto frame = this->m_browser->GetMainFrame ();
	if (frame) {
	    std::ostringstream js;
	    int pad = this->m_panelPaddingLeft + 14; // extra margin for visual clearance
	    js << "(function(){"
	       << "function inject(){"
	       << "if(!document.head){setTimeout(inject,50);return;}"
	       << "if(document.getElementById('__wf_panel_css'))return;"
	       << "var s=document.createElement('style');"
	       << "s.id='__wf_panel_css';"
	       << "s.textContent='"
	       << ".container{padding-left:" << pad << "px !important;box-sizing:border-box !important;width:100vw !important}"
	       << " .bottom-row{width:auto !important}"
	       << " .top-row{width:auto !important}"
	       << "';"
	       << "document.head.appendChild(s);"
	       << "}"
	       << "inject();"
	       << "})();";
	    frame->ExecuteJavaScript (js.str (), frame->GetURL (), 0);
	    this->m_cssInjected = true;
	    std::cerr << "[CWeb] Injected CSS: padding-left " << pad << "px" << std::endl;
	}
    }

    // use the scene's framebuffer by default
    glBindFramebuffer (GL_FRAMEBUFFER, this->getWallpaperFramebuffer ());
    // ensure we render over the whole framebuffer
    glViewport (0, 0, this->getWidth (), this->getHeight ());

    // Cef processes all messages, including OnPaint, which renders frame
    // If there is no OnPaint in message loop, we will not update(render) frame
    //  This means some frames will not have OnPaint call in cef messageLoop
    //  Because of that glClear will result in flickering on higher fps
    //  Do not use glClear until some method to control rendering with cef is supported
    // We might actually try to use cef to execute javascript, and not using off-screen rendering at all
    // But for now let it be like this
    //  glClear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    CefDoMessageLoopWork ();
}

void CWeb::updateMouse (const glm::ivec4& viewport) {
    // update virtual mouse position first
    auto& input = this->getContext ().getInputContext ().getMouseInput ();

    const glm::dvec2 position = input.position ();
    const auto leftClick = input.leftClick ();
    const auto rightClick = input.rightClick ();

    // Raw viewport-space coordinates (Y flipped from OpenGL to screen/CEF)
    const int rawX = std::clamp (static_cast<int> (position.x - viewport.x), 0, viewport.z);
    const int rawY = viewport.w - std::clamp (static_cast<int> (position.y - viewport.y), 0, viewport.w);

    // Convert viewport coordinates to CEF coordinates, accounting for ZoomFill
    const float cefW = static_cast<float> (this->getWidth ());
    const float cefH = static_cast<float> (this->getHeight ());
    const float vpW = static_cast<float> (viewport.z);
    const float vpH = static_cast<float> (viewport.w);

    const float scale = std::max (vpW / cefW, vpH / cefH);
    const float offsetX = (cefW * scale - vpW) / 2.0f;
    const float offsetY = (cefH * scale - vpH) / 2.0f;

    CefMouseEvent evt;
    evt.x = std::clamp (static_cast<int> ((rawX + offsetX) / scale), 0, static_cast<int> (cefW) - 1);
    evt.y = std::clamp (static_cast<int> ((rawY + offsetY) / scale), 0, static_cast<int> (cefH) - 1);

    // Send mouse position to cef
    this->m_browser->GetHost ()->SendMouseMoveEvent (evt, false);

    // TODO: ANY OTHER MOUSE EVENTS TO SEND?
    if (leftClick != this->m_leftClick) {
	this->m_browser->GetHost ()->SendMouseClickEvent (
	    evt, CefBrowserHost::MouseButtonType::MBT_LEFT,
	    leftClick == WallpaperEngine::Input::MouseClickStatus::Released, 1
	);
    }

    if (rightClick != this->m_rightClick) {
	this->m_browser->GetHost ()->SendMouseClickEvent (
	    evt, CefBrowserHost::MouseButtonType::MBT_RIGHT,
	    rightClick == WallpaperEngine::Input::MouseClickStatus::Released, 1
	);
    }

    this->m_leftClick = leftClick;
    this->m_rightClick = rightClick;
}

CWeb::~CWeb () {
    CefDoMessageLoopWork ();
    this->m_browser->GetHost ()->CloseBrowser (true);

    delete this->m_renderHandler;
}

int CWeb::detectPanelPaddingLeft () {
    // Try to read _NET_WORKAREA from X11 root window to detect left panel/taskbar offset.
    // On Wayland without XWayland this will fail gracefully and return 0.
    Display* dpy = XOpenDisplay (nullptr);
    if (!dpy)
	return 0;

    Atom atom = XInternAtom (dpy, "_NET_WORKAREA", True);
    if (atom == None) {
	XCloseDisplay (dpy);
	return 0;
    }

    Atom actualType;
    int actualFormat;
    unsigned long nItems, bytesAfter;
    unsigned char* data = nullptr;

    int padding = 0;
    if (XGetWindowProperty (dpy, DefaultRootWindow (dpy), atom, 0, 4, False, XA_CARDINAL, &actualType, &actualFormat,
			    &nItems, &bytesAfter, &data) == Success &&
	data && nItems >= 1) {
	padding = static_cast<int> (reinterpret_cast<unsigned long*> (data)[0]);
	XFree (data);
    }

    XCloseDisplay (dpy);
    std::cerr << "[CWeb] Detected panel padding left: " << padding << "px" << std::endl;
    return padding;
}
