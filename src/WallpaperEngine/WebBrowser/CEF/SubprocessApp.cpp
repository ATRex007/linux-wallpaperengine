#include "SubprocessApp.h"
#include "WPSchemeHandlerFactory.h"
#include "WallpaperEngine/Data/Model/Project.h"

#include "include/cef_v8.h"

using namespace WallpaperEngine::WebBrowser::CEF;

SubprocessApp::SubprocessApp (WallpaperEngine::Application::WallpaperApplication& application) :
    m_application (application) {
    for (const auto& info : this->m_application.getBackgrounds () | std::views::values) {
	this->m_handlerFactories[info->workshopId] = new WPSchemeHandlerFactory (*info);
    }
}

void SubprocessApp::OnRegisterCustomSchemes (CefRawPtr<CefSchemeRegistrar> registrar) {
    // register all the needed schemes, "wp" + the background id is going to be our scheme
    for (const auto& workshopId : this->m_handlerFactories | std::views::keys) {
	registrar->AddCustomScheme (
	    WPSchemeHandlerFactory::generateSchemeName (workshopId),
	    CEF_SCHEME_OPTION_STANDARD | CEF_SCHEME_OPTION_SECURE | CEF_SCHEME_OPTION_FETCH_ENABLED
	    | CEF_SCHEME_OPTION_CORS_ENABLED
	);
    }
}

void SubprocessApp::OnContextCreated (
    CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame, CefRefPtr<CefV8Context> context
) {
    // Only inject into the main frame
    if (!frame->IsMain ()) return;

    // Inject the Wallpaper Engine JavaScript API that web wallpapers expect.
    // Without this, widgets (audio players, timers, etc.) fail because
    // window.wallpaperRegisterAudioListener and friends are undefined.
    frame->ExecuteJavaScript (R"JS(

// --- Wallpaper Engine JavaScript API ---

// Audio visualizer listener
window.wallpaperRegisterAudioListener = function(callback) {
    window._wpAudioCallback = callback;
};

// Property listeners
window.wallpaperPropertyListener = {
    applyUserProperties: null,
    applyGeneralProperties: null,
    setPaused: null,
    userDirectoryFilesAddedOrChanged: null,
    userDirectoryFilesRemoved: null
};

// Media integration listeners
window.wallpaperRegisterMediaStatusListener = function(callback) {
    window._wpMediaStatusCallback = callback;
};
window.wallpaperRegisterMediaPropertiesListener = function(callback) {
    window._wpMediaPropertiesCallback = callback;
};
window.wallpaperRegisterMediaThumbnailListener = function(callback) {
    window._wpMediaThumbnailCallback = callback;
};
window.wallpaperRegisterMediaTimelineListener = function(callback) {
    window._wpMediaTimelineCallback = callback;
};
window.wallpaperRegisterMediaPlaybackListener = function(callback) {
    window._wpMediaPlaybackCallback = callback;
};

// Permission request — auto-grant all permissions
window.wallpaperRequestPermission = function(permission) {
    return Promise.resolve(true);
};

// Signal that the engine is ready
window.wallpaperEngineReady = true;

)JS", frame->GetURL (), 0);
}

const WallpaperEngine::Application::WallpaperApplication& SubprocessApp::getApplication () const {
    return this->m_application;
}

const std::map<std::string, WPSchemeHandlerFactory*>& SubprocessApp::getHandlerFactories () const {
    return this->m_handlerFactories;
}