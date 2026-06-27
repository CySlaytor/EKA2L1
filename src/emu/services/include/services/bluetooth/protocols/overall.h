#pragma once

#include <services/bluetooth/btmidman.h>

namespace eka2l1 {
    class socket_server;
}

namespace eka2l1::epoc::bt {
    void add_bluetooth_stack_protocols(socket_server *sock, epoc::bt::midman *mm, const bool oldarch);
}