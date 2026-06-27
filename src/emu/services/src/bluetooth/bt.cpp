#include <services/bluetooth/bt.h>
#include <services/bluetooth/protocols/overall.h>
#include <services/socket/server.h>

#include <config/config.h>
#include <system/epoc.h>
#include <utils/err.h>

namespace eka2l1 {
    bt_server::bt_server(eka2l1::system *sys)
        : service::typical_server(sys, "BTServer") {
    }

    void bt_server::connect(service::ipc_context &context) {
        create_session<bt_client_session>(&context);
        context.complete(epoc::error_none);
    }

    bt_client_session::bt_client_session(service::typical_server *serv, const kernel::uid ss_id,
        epoc::version client_version)
        : service::typical_session(serv, ss_id, client_version) {
    }

    void bt_client_session::fetch(service::ipc_context *ctx) {
        LOG_ERROR(SERVICE_BLUETOOTH, "Unimplemented opcode for Bluetooth server 0x{:X}", ctx->msg->function);
        ctx->complete(epoc::error_none);
    }
}