#pragma once

namespace eka2l1 {
    class socket_server;
}

namespace eka2l1::epoc::internet {
    void add_internet_stack_protocols(socket_server *sock, const bool oldarch);
}