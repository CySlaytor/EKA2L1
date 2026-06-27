#pragma once

#include <kernel/server.h>
#include <services/framework.h>

namespace eka2l1 {

    enum connmonitor_opcode {
        connmonitor_get_connection_count = 0,
        connmonitor_get_connection_info = 1,
        connmonitor_get_int_attribute = 3,
        connmonitor_get_uint_attribute = 4,
        connmonitor_receive_event = 14,
        connmonitor_cancel_receive_event = 15
    };

    enum bearer_id {
        bearer_id_gprs = 2000000
    };

    class connmonitor_server : public service::typical_server {
    public:
        explicit connmonitor_server(eka2l1::system *sys);
        void connect(service::ipc_context &context) override;
    };

    struct connmonitor_client_session : public service::typical_session {
        epoc::notify_info nof_info;

        explicit connmonitor_client_session(service::typical_server *serv, const kernel::uid ss_id, epoc::version client_version);

        void fetch(service::ipc_context *ctx) override;
        void get_connection_count(eka2l1::service::ipc_context *ctx);
        void receive_event(eka2l1::service::ipc_context *ctx);
        void get_int_attribute(eka2l1::service::ipc_context *ctx);
        void get_uint_attribute(eka2l1::service::ipc_context *ctx);
        void get_connection_info(eka2l1::service::ipc_context *ctx);
        void cancel_receive_event(eka2l1::service::ipc_context *ctx);
    };
}