#include <kernel/kernel.h>
#include <qt/utils.h>
#include <services/window/window.h>
#include <system/epoc.h>

eka2l1::window_server *get_window_server_through_system(eka2l1::system *sys) {
    eka2l1::kernel_system *kernel = sys->get_kernel_system();
    if (!kernel) {
        return nullptr;
    }

    const std::string win_server_name = eka2l1::get_winserv_name_by_epocver(kernel->get_epoc_version());
    return reinterpret_cast<eka2l1::window_server *>(kernel->get_by_name<eka2l1::service::server>(win_server_name));
}

eka2l1::epoc::screen *get_current_active_screen(eka2l1::system *sys, const int provided_num) {
    int active_screen_number = (provided_num < 0) ? 0 : provided_num;

    eka2l1::window_server *server = get_window_server_through_system(sys);
    if (server) {
        eka2l1::epoc::screen *scr = server->get_screens();
        while (scr) {
            if (scr->number == active_screen_number) {
                return scr;
            }
            scr = scr->next;
        }
    }

    return nullptr;
}

namespace eka2l1::common {
    // Silently ignore browser launch requests from the game
    bool launch_browser(const std::string &url) {
        return true;
    }
}