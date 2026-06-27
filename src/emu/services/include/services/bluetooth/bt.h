#pragma once

#include <services/bluetooth/btmidman.h>

#include <kernel/server.h>
#include <services/framework.h>

namespace eka2l1 {
    enum bt_server_opcode {
        bt_server_turn_on = 0,
        bt_server_turn_off = 1
    };

    class bt_server : public service::typical_server {
    public:
        explicit bt_server(eka2l1::system *sys);
        void connect(service::ipc_context &context) override;
    };

    struct bt_client_session : public service::typical_session {
        explicit bt_client_session(service::typical_server *serv, const kernel::uid ss_id, epoc::version client_version);
        void fetch(service::ipc_context *ctx) override;
    };
}