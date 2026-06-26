#pragma once
#include <string>

namespace eka2l1 {
    class window_server;
    class system;

    namespace epoc {
        struct screen;
    }
}

static constexpr const char *SHOW_LOG_CONSOLE_SETTING_NAME = "showLogConsole";

// Fetch the window server and the active screen from the Symbian backend
eka2l1::window_server *get_window_server_through_system(eka2l1::system *sys);
eka2l1::epoc::screen *get_current_active_screen(eka2l1::system *sys, const int provided_num = -1);

namespace eka2l1::common {
    // Stubbed browser launcher to satisfy epocservs linker dependencies
    bool launch_browser(const std::string &url);
}