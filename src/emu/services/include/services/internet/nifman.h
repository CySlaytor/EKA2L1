#pragma once

#include <kernel/server.h>
#include <services/framework.h>
#include <services/socket/connection.h>

namespace eka2l1 {
    class socket_server;

    class nifman_server : public service::typical_server {
    protected:
        socket_server *sock_serv_;

    public:
        explicit nifman_server(eka2l1::system *sys);
    };
}