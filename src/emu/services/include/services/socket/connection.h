#pragma once

#include <common/container.h>
#include <services/socket/common.h>

#include <cstdint>
#include <functional>
#include <memory>
#include <string>

namespace eka2l1 {
    class socket_server;
}

namespace eka2l1::epoc::socket {
    struct protocol;
    struct socket;

    struct conn_preferences {
        std::uint32_t reserved_;
    };

    enum setting_type {
        setting_type_bool,
        setting_type_int,
        setting_type_des
    };

    struct connection {
    protected:
        protocol *pr_;
        socket *sock_;
        saddress dest_;
    };

    struct connect_agent {
    protected:
        socket_server *sock_serv_;

    public:
        explicit connect_agent(socket_server *serv)
            : sock_serv_(serv) {}
        virtual std::u16string agent_name() const = 0;
        virtual std::unique_ptr<connection> start_connection(conn_preferences &prefs) = 0;
    };
}