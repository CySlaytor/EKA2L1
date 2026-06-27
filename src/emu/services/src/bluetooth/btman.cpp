#include <services/bluetooth/btman.h>
#include <services/bluetooth/protocols/overall.h>
#include <services/socket/server.h>

#include <config/config.h>
#include <system/epoc.h>
#include <utils/err.h>

namespace eka2l1 {
    std::string get_btman_server_name_by_epocver(const epocver ver) {
        if (ver <= epocver::epoc80) {
            return "BTManServer";
        }

        return "!BTManServer";
    }

    btman_server::btman_server(eka2l1::system *sys)
        : service::typical_server(sys, get_btman_server_name_by_epocver(sys->get_symbian_version_use())) {
        config::state *conf = sys->get_config();
        mid_ = epoc::bt::make_bluetooth_midman(*conf);

        socket_server *ssock = reinterpret_cast<socket_server *>(kern->get_by_name<service::server>(
            get_socket_server_name_by_epocver(kern->get_epoc_version())));

        if (conf) {
            mid_->device_name(common::utf8_to_ucs2(conf->device_display_name));
        }

        if (ssock) {
            epoc::bt::add_bluetooth_stack_protocols(ssock, mid_.get(), is_oldarch());
        }
    }

    void btman_server::connect(service::ipc_context &context) {
        create_session<btman_client_session>(&context);
        context.complete(epoc::error_none);
    }

    void btman_server::device_name(const std::u16string &new_name) {
        mid_->device_name(new_name);
    }

    bool btman_server::is_oldarch() {
        return (kern->get_epoc_version() < epocver::eka2);
    }

    btman_client_session::btman_client_session(service::typical_server *serv, const kernel::uid ss_id,
        epoc::version client_version)
        : service::typical_session(serv, ss_id, client_version) {
    }

    void btman_client_session::fetch(service::ipc_context *ctx) {
        LOG_ERROR(SERVICE_BLUETOOTH, "Unimplemented opcode for Btman server 0x{:X}", ctx->msg->function);
        ctx->complete(epoc::error_none);
    }
}