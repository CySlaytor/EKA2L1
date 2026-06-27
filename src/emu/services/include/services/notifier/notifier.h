#pragma once

#include <kernel/server.h>
#include <services/framework.h>
#include <services/notifier/plugin.h>

namespace eka2l1 {
    enum notifier_opcode {
        notifier_start = 2,
        notifier_cancel = 3,
        notifier_update = 4,
        notfiier_start_and_get_response = 5,
    };

    std::string get_notifier_server_name_by_epocver(const epocver ver);

    class notifier_server : public service::typical_server {
        std::vector<epoc::notifier::plugin_instance> plugins_;

    public:
        explicit notifier_server(eka2l1::system *sys);

        epoc::notifier::plugin_base *get_plugin(const epoc::uid id);
        void connect(service::ipc_context &context) override;
    };

    struct notifier_client_session : public service::typical_session {
        explicit notifier_client_session(service::typical_server *serv, const kernel::uid ss_id, epoc::version client_version);

        void fetch(service::ipc_context *ctx) override;
        void start_notifier(service::ipc_context *ctx);
    };
}