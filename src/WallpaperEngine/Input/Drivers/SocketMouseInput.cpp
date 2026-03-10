#include "SocketMouseInput.h"

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>

using namespace WallpaperEngine::Input::Drivers;

static std::string getSocketPath () {
    const char* xdg = getenv ("XDG_RUNTIME_DIR");
    if (xdg)
        return std::string (xdg) + "/wallforge-input.sock";
    return "/tmp/wallforge-input.sock";
}

SocketMouseInput::SocketMouseInput (int screenWidth, int screenHeight) :
    m_screenWidth (screenWidth),
    m_screenHeight (screenHeight),
    m_socketPath (getSocketPath ()) {
    // Remove stale socket file
    unlink (m_socketPath.c_str ());

    m_fd = socket (AF_UNIX, SOCK_DGRAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
    if (m_fd < 0) {
        std::cerr << "[SocketMouseInput] socket() failed: " << strerror (errno) << std::endl;
        return;
    }

    struct sockaddr_un addr {};
    addr.sun_family = AF_UNIX;

    if (m_socketPath.size () >= sizeof (addr.sun_path)) {
        std::cerr << "[SocketMouseInput] Socket path too long" << std::endl;
        close (m_fd);
        m_fd = -1;
        return;
    }
    strncpy (addr.sun_path, m_socketPath.c_str (), sizeof (addr.sun_path) - 1);

    if (bind (m_fd, reinterpret_cast<struct sockaddr*> (&addr), sizeof (addr)) < 0) {
        std::cerr << "[SocketMouseInput] bind() failed: " << strerror (errno) << std::endl;
        close (m_fd);
        m_fd = -1;
        return;
    }

    // Allow group/other to send to this socket
    chmod (m_socketPath.c_str (), 0666);

    std::cerr << "[SocketMouseInput] Listening on " << m_socketPath << std::endl;
}

SocketMouseInput::~SocketMouseInput () {
    if (m_fd >= 0) {
        close (m_fd);
        unlink (m_socketPath.c_str ());
    }
}

void SocketMouseInput::update () {
    if (m_fd < 0)
        return;

    // Drain all pending datagrams, keeping only the latest state
    char buf [128];
    ssize_t n;
    while ((n = recv (m_fd, buf, sizeof (buf) - 1, 0)) > 0) {
        buf [n] = '\0';

        if (buf [0] == 'M') {
            // Move: "M <x> <y>"
            double x, y;
            if (sscanf (buf, "M %lf %lf", &x, &y) == 2) {
                // Convert from screen coords (top-left origin) to OpenGL coords (bottom-left origin)
                m_position.x = x;
                m_position.y = static_cast<double> (m_screenHeight) - y;
            }
        } else if (buf [0] == 'C') {
            // Click: "C <button> <pressed> <x> <y>"
            int button, pressed;
            double x, y;
            if (sscanf (buf, "C %d %d %lf %lf", &button, &pressed, &x, &y) == 4) {
                m_position.x = x;
                m_position.y = static_cast<double> (m_screenHeight) - y;

                auto status = pressed ? MouseClickStatus::Clicked : MouseClickStatus::Released;
                if (button == 0)
                    m_leftClick = status;
                else if (button == 1)
                    m_rightClick = status;
            }
        }
    }
}

glm::dvec2 SocketMouseInput::position () const {
    return m_position;
}

WallpaperEngine::Input::MouseClickStatus SocketMouseInput::leftClick () const {
    return m_leftClick;
}

WallpaperEngine::Input::MouseClickStatus SocketMouseInput::rightClick () const {
    return m_rightClick;
}
