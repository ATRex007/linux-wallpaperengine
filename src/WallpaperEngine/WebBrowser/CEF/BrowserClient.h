#pragma once

#include "include/cef_client.h"
#include "include/cef_display_handler.h"
#include "include/cef_load_handler.h"

namespace WallpaperEngine::WebBrowser::CEF {
// *************************************************************************
//! \brief Provide access to browser-instance-specific callbacks. A single
//! CefClient instance can be shared among any number of browsers.
// *************************************************************************
class BrowserClient : public CefClient, public CefDisplayHandler, public CefLoadHandler {
public:
    explicit BrowserClient (CefRefPtr<CefRenderHandler> ptr, int contentPaddingLeft = 0);

    [[nodiscard]] CefRefPtr<CefRenderHandler> GetRenderHandler () override;
    [[nodiscard]] CefRefPtr<CefDisplayHandler> GetDisplayHandler () override { return this; }
    [[nodiscard]] CefRefPtr<CefLoadHandler> GetLoadHandler () override { return this; }

    // CefDisplayHandler — forward JS console messages to stderr for debugging
    bool OnConsoleMessage (
        CefRefPtr<CefBrowser> browser, cef_log_severity_t level, const CefString& message,
        const CefString& source, int line
    ) override;

    // CefLoadHandler — log load errors and send focus on load completion
    void OnLoadError (
        CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame, ErrorCode errorCode,
        const CefString& errorText, const CefString& failedUrl
    ) override;
    void OnLoadEnd (CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame, int httpStatusCode) override;
    void OnLoadingStateChange (
        CefRefPtr<CefBrowser> browser, bool isLoading, bool canGoBack, bool canGoForward
    ) override;

    CefRefPtr<CefRenderHandler> m_renderHandler = nullptr;
    int m_contentPaddingLeft = 0;

    IMPLEMENT_REFCOUNTING (BrowserClient);
};
} // namespace WallpaperEngine::WebBrowser::CEF