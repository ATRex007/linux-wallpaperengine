/**
 * WallForge - Dynamic Wallpaper Engine for Linux
 * Based on linux-wallpaperengine (GPLv3)
 *
 * wallforge_main.cpp - WallForge CLI entry point
 */

#include "WallForge/Core/WallForgeApp.h"

#include <iostream>

int main(int argc, char* argv[]) {
    WallForge::WallForgeApp app;

    if (!app.init()) {
        std::cerr << "Failed to initialize WallForge" << std::endl;
        return 1;
    }

    return app.processCommand(argc, argv);
}
