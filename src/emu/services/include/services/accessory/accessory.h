#pragma once

#include <common/container.h>
#include <memory>
#include <services/framework.h>

namespace eka2l1 {
    class accessory_server : public service::typical_server {
    public:
        explicit accessory_server(eka2l1::system *sys);
        void connect(service::ipc_context &context) override;
    };

    struct accessory_subsession {
    protected:
        accessory_server *serv_;

    public:
        explicit accessory_subsession(accessory_server *svr);
        virtual bool fetch(service::ipc_context *ctx) = 0;
    };

    struct accessory_single_connection_subsession : public accessory_subsession {
    protected:
        epoc::notify_info accessory_connected_nof_;

    public:
        explicit accessory_single_connection_subsession(accessory_server *svr);

        bool fetch(service::ipc_context *ctx) override;
        void notify_new_accessory_connected(service::ipc_context *ctx);
        void cancel_notify_new_accessory_connected(service::ipc_context *ctx);
        void get_accessory_connection_status(service::ipc_context *ctx);
    };

    using accessory_subsession_instance = std::unique_ptr<accessory_subsession>;

    struct accessory_session : public service::typical_session {
    private:
        common::identity_container<accessory_subsession_instance> subsessions_;

    public:
        explicit accessory_session(service::typical_server *serv, const kernel::uid ss_id, epoc::version client_version);

        void fetch(service::ipc_context *ctx) override;
        void create_accessory_single_connection_subsession(service::ipc_context *ctx);
    };
}