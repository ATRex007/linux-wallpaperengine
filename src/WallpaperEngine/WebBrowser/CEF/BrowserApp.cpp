#include "BrowserApp.h"
#include "WallpaperEngine/Logging/Log.h"

using namespace WallpaperEngine::WebBrowser::CEF;

BrowserApp::BrowserApp (WallpaperEngine::Application::WallpaperApplication& application) :
    SubprocessApp (application) { }

CefRefPtr<CefBrowserProcessHandler> BrowserApp::GetBrowserProcessHandler () { return this; }

void BrowserApp::OnContextInitialized () {
    // register all the needed schemes, "wp" + the background id is going to be our scheme
    for (const auto& [workshopId, factory] : this->getHandlerFactories ()) {
	CefRegisterSchemeHandlerFactory (
	    WPSchemeHandlerFactory::generateSchemeName (workshopId), static_cast<const char*> (nullptr), factory
	);
    }
}

void BrowserApp::OnBeforeCommandLineProcessing (const CefString& process_type, CefRefPtr<CefCommandLine> command_line) {
    command_line->AppendSwitchWithValue (
	"--disable-features",
	"IsolateOrigins,HardwareMediaKeyHandling,WebContentsOcclusion,RendererCodeIntegrityEnabled,site-per-process"
    );
    command_line->AppendSwitch ("--disable-gpu-shader-disk-cache");
    command_line->AppendSwitch ("--disable-site-isolation-trials");
    command_line->AppendSwitch ("--disable-web-security");
    command_line->AppendSwitchWithValue ("--remote-allow-origins", "*");
    command_line->AppendSwitchWithValue ("--autoplay-policy", "no-user-gesture-required");
    command_line->AppendSwitch ("--disable-background-timer-throttling");
    command_line->AppendSwitch ("--disable-backgrounding-occluded-windows");
    command_line->AppendSwitch ("--disable-background-media-suspend");
    command_line->AppendSwitch ("--disable-renderer-backgrounding");
    command_line->AppendSwitch ("--disable-breakpad");
    command_line->AppendSwitch ("--disable-field-trial-config");
    command_line->AppendSwitch ("--no-experiments");
    command_line->AppendSwitch ("--no-sandbox");
    command_line->AppendSwitch ("--no-zygote");
    command_line->AppendSwitch ("--disable-setuid-sandbox");
}

void BrowserApp::OnBeforeChildProcessLaunch (CefRefPtr<CefCommandLine> command_line) {
    // Pass workshop IDs to child processes so they can register custom schemes.
    // Without scheme registration, the network service can't process wp:// URLs,
    // causing Mojo VALIDATION_ERROR_DESERIALIZATION_FAILED errors.
    std::string workshopIds;
    for (const auto& workshopId : this->getHandlerFactories () | std::views::keys) {
	if (!workshopIds.empty ()) workshopIds += ",";
	workshopIds += workshopId;
    }
    if (!workshopIds.empty ()) {
	command_line->AppendSwitchWithValue ("workshop-ids", workshopIds);
    }

    // Only pass through specific arguments needed by the engine's subprocess detection.
    // Do NOT pass all original arguments - CEF child processes would misinterpret
    // wallpaper engine flags (--assets-dir, --window, wallpaper paths...) causing crashes.
    const auto& ctx = this->getApplication ().getContext ();
    const int argc = ctx.getArgc ();
    char** argv = ctx.getArgv ();

    // Flags that CEF subprocesses need (with their values)
    static const std::vector<std::string> passthrough_flags = {
	"--assets-dir",
    };

    for (int i = 1; i < argc; i++) {
	std::string arg = argv[i];

	// Check if this is a passthrough flag with a value
	bool found = false;
	for (const auto& flag : passthrough_flags) {
	    if (arg == flag && (i + 1) < argc) {
		command_line->AppendSwitchWithValue (flag.substr (2), argv[i + 1]);
		i++; // skip value
		found = true;
		break;
	    }
	}

	if (found) continue;

	// Skip flags that would confuse CEF subprocesses
	if (arg == "--fps" || arg == "--window" || arg == "--screen-root" ||
	    arg == "--scaling" || arg == "--clamp" || arg == "--silent") {
	    if ((i + 1) < argc && argv[i + 1][0] != '-') {
		i++; // skip the value too
	    }
	    continue;
	}

	// Skip positional arguments (wallpaper paths)
	if (arg[0] != '-') {
	    continue;
	}
    }
}