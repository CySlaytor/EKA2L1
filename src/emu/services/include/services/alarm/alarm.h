#pragma once

#include <services/framework.h>

namespace eka2l1 {
    enum alarm_opcode {
        alarm_get_alarm_id_list_by_state = 11,
        alarm_notify_change = 19,
        alarm_notify_change_cancel = 20,
        alarm_fetch_transfer_buffer = 21
    };

    class alarm_server : public service::typical_server {
    public:
        explicit alarm_server(eka2l1::system *sys);
        void connect(service::ipc_context &context) override;
    };

    struct alarm_session : public service::typical_session {
        std::vector<std::uint8_t> transfer_buf;
        std::vector<std::int32_t> alarm_ids;

        epoc::notify_info change_notify_info;
        std::uint8_t *change_alarm_id_fill;

    public:
        explicit alarm_session(service::typical_server *serv, const kernel::uid ss_id, epoc::version client_version);

        void fetch(service::ipc_context *ctx) override;
        void get_alarm_id_list_by_state(service::ipc_context *ctx);
        void fetch_transfer_buffer(service::ipc_context *ctx);
        void notify_change(service::ipc_context *ctx);
        void notify_change_cancel(service::ipc_context *ctx);
    };
}