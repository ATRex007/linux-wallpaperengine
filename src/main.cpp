#include <csignal>
#include <cstring>
#include <iostream>

#include "WallpaperEngine/Application/ApplicationContext.h"
#include "WallpaperEngine/Application/WallpaperApplication.h"
#include "WallpaperEngine/Logging/Log.h"
#include "include/cef_app.h"
#include "include/cef_command_line.h"
#include "include/cef_scheme.h"
#include "WallpaperEngine/WebBrowser/CEF/SubprocessApp.h"

// Minimal CefApp for child processes that registers custom wp:// schemes
// without needing the full WallpaperApplication. Workshop IDs are passed
// via --workshop-ids command-line flag from the browser process.
class MinimalSubprocessApp : public CefApp {
  public:
    MinimalSubprocessApp (int argc, char* argv[]) {
	// Parse --workshop-ids from command line
	const std::string prefix = "--workshop-ids=";
	for (int i = 1; i < argc; i++) {
	    std::string arg = argv[i];
	    if (arg.substr (0, prefix.size ()) == prefix) {
		std::string ids = arg.substr (prefix.size ());
		size_t pos = 0;
		while ((pos = ids.find (',')) != std::string::npos) {
		    m_workshopIds.push_back (ids.substr (0, pos));
		    ids.erase (0, pos + 1);
		}
		if (!ids.empty ()) m_workshopIds.push_back (ids);
		break;
	    }
	}
    }

    void OnRegisterCustomSchemes (CefRawPtr<CefSchemeRegistrar> registrar) override {
	for (const auto& id : m_workshopIds) {
	    std::string schemeName = std::string ("wp") + id;
	    registrar->AddCustomScheme (
		schemeName, CEF_SCHEME_OPTION_STANDARD | CEF_SCHEME_OPTION_SECURE | CEF_SCHEME_OPTION_FETCH_ENABLED
	    );
	}
    }

  private:
    std::vector<std::string> m_workshopIds;
    IMPLEMENT_REFCOUNTING (MinimalSubprocessApp);
    DISALLOW_COPY_AND_ASSIGN (MinimalSubprocessApp);
};

WallpaperEngine::Application::WallpaperApplication* app;

void signalhandler (const int sig) {
    if (app == nullptr) {
	return;
    }

    app->signal (sig);
}

void initLogging () {
    sLog.addOutput (new std::ostream (std::cout.rdbuf ()));
    sLog.addError (new std::ostream (std::cerr.rdbuf ()));
}

/**
 * Detect if this process is a CEF subprocess (zygote, utility, renderer, gpu-process).
 * If so, run CefExecuteProcess immediately and exit, before parsing wallpaper engine arguments.
 */
static bool isCefSubprocess (int argc, char* argv[]) {
    const std::string typePrefix = "--type=";
    for (int i = 1; i < argc; i++) {
	if (strncmp (typePrefix.c_str (), argv[i], typePrefix.size ()) == 0) {
	    return true;
	}
    }
    return false;
}

int main (int argc, char* argv[]) {
    try {
	// Early CEF subprocess detection: if this is a CEF child process,
	// run CefExecuteProcess immediately and exit without parsing engine args.
	if (isCefSubprocess (argc, argv)) {
	    CefMainArgs main_args (argc, argv);
	    CefRefPtr<CefApp> subprocess_app = new MinimalSubprocessApp (argc, argv);
	    const int exit_code = CefExecuteProcess (main_args, subprocess_app, nullptr);
	    return exit_code >= 0 ? exit_code : 1;
	}

	// Main process flow
	bool enableLogging = true;

	if (enableLogging) {
	    initLogging ();
	}

	WallpaperEngine::Application::ApplicationContext appContext (argc, argv);

	appContext.loadSettingsFromArgv ();

	app = new WallpaperEngine::Application::WallpaperApplication (appContext);

	// halt if the list-properties option was specified
	if (appContext.settings.general.onlyListProperties) {
	    delete app;
	    return 0;
	}

	// attach signals to gracefully stop
	std::signal (SIGINT, signalhandler);
	std::signal (SIGTERM, signalhandler);
	std::signal (SIGKILL, signalhandler);

	// show the wallpaper application
	app->show ();

	// remove signal handlers before destroying app
	std::signal (SIGINT, SIG_DFL);
	std::signal (SIGTERM, SIG_DFL);
	std::signal (SIGKILL, SIG_DFL);

	delete app;

	return 0;
    } catch (const std::exception& e) {
	std::cerr << e.what () << std::endl;
	return 1;
    }
}