#include <services/internet/protocols/inet.h>
#include <services/internet/protocols/overall.h>
#include <services/socket/server.h>

#include <common/log.h>
#include <common/thread.h>

namespace eka2l1::epoc::internet {
    inet_bridged_protocol::inet_bridged_protocol(kernel_system *kern, const bool oldarch)
        : socket::protocol(oldarch)
        , looper_(libuv::default_looper)
        , kern_(kern) {
    }

    void add_internet_stack_protocols(socket_server *sock, const bool oldarch) {
        std::unique_ptr<epoc::socket::protocol> inet_br_pr = std::make_unique<inet_bridged_protocol>(
            sock->get_kernel_object_owner(), oldarch);

        if (!sock->add_protocol(inet_br_pr)) {
            LOG_ERROR(SERVICE_BLUETOOTH, "Failed to add INET bridged protocol");
        }
    }
}