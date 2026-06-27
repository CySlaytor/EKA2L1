#include <services/shutdown/shutdown.h>
#include <system/epoc.h>

namespace eka2l1 {
    std::string get_shutdown_server_name_through_epocver(const epocver ver) {
        if (ver < epocver::eka2) {
            return "ShutdownServer";
        }

        return "!ShutdownServer";
    }
}