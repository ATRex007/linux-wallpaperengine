#include "BrowserClient.h"
#include <iostream>
#include <sstream>

using namespace WallpaperEngine::WebBrowser::CEF;

BrowserClient::BrowserClient (CefRefPtr<CefRenderHandler> ptr, int contentPaddingLeft) :
    m_renderHandler (std::move (ptr)), m_contentPaddingLeft (contentPaddingLeft) { }

CefRefPtr<CefRenderHandler> BrowserClient::GetRenderHandler () { return m_renderHandler; }

bool BrowserClient::OnConsoleMessage (
    CefRefPtr<CefBrowser> browser, cef_log_severity_t level, const CefString& message, const CefString& source,
    int line
) {
    const char* levelStr = "INFO";
    switch (level) {
	case LOGSEVERITY_WARNING: levelStr = "WARN"; break;
	case LOGSEVERITY_ERROR: levelStr = "ERROR"; break;
	case LOGSEVERITY_FATAL: levelStr = "FATAL"; break;
	default: break;
    }
    std::cerr << "[CEF-JS:" << levelStr << "] " << message.ToString ()
              << " (" << source.ToString () << ":" << line << ")" << std::endl;
    return false;
}

void BrowserClient::OnLoadError (
    CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame, ErrorCode errorCode, const CefString& errorText,
    const CefString& failedUrl
) {
    std::cerr << "[CEF-LoadError] " << errorText.ToString () << " url=" << failedUrl.ToString ()
              << " code=" << errorCode << std::endl;
}

void BrowserClient::OnLoadEnd (CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame, int httpStatusCode) {
    if (frame->IsMain ()) {
	// Grant focus so interactive elements (buttons, sliders) respond to clicks
	browser->GetHost ()->SetFocus (true);
    }
}

void BrowserClient::OnLoadingStateChange (
    CefRefPtr<CefBrowser> browser, bool isLoading, bool canGoBack, bool canGoForward
) {
}