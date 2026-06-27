#pragma once

#include <services/framework.h>

#include <kernel/server.h>
#include <utils/des.h>
#include <utils/reqsts.h>

#include <vector>

namespace eka2l1 {
    enum drm_notifier_opcode {
        drm_notifier_listen_for_event = 0x2,
        drm_notifier_register_accept_event = 0x3,
        drm_notifier_unregister_accept_event = 0x4,
        drm_notifier_cancel_listen_for_event = 0xFF
    };

    struct drm_accept_event_type {
        std::uint32_t event_type_;
        std::string content_uri_;
    };

    class drm_notifier_server : public service::typical_server {
    public:
        explicit drm_notifier_server(eka2l1::system *sys);
        void connect(service::ipc_context &context) override;
    };

    struct drm_notifier_client_session : public service::typical_session {
    private:
        epoc::notify_info notify_;
        std::uint8_t *to_write_data_;
        epoc::des8 *to_write_event_type_;

        std::vector<drm_accept_event_type> accept_types_;

    protected:
        bool listen(epoc::notify_info &info, std::uint8_t *data_to_write, epoc::des8 *event_type_to_write);

        void listen_for_event(service::ipc_context *ctx);
        void cancel_listen_for_event(service::ipc_context *ctx);
        void register_event(service::ipc_context *ctx);
        void unregister_event(service::ipc_context *ctx);

    public:
        explicit drm_notifier_client_session(service::typical_server *serv, const kernel::uid ss_id, epoc::version client_version);

        void fetch(service::ipc_context *ctx) override;
        bool register_event(const std::uint32_t type, const std::string uri);
        bool unregister_event(const std::uint32_t type, const std::string uri);
    };
}