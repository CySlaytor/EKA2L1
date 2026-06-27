#include <services/socket/common.h>
#include <services/socket/connection.h>
#include <services/socket/server.h>
#include <services/socket/socket.h>

#include <common/log.h>
#include <system/epoc.h>
#include <utils/err.h>

namespace eka2l1::epoc::socket {
    socket_connection_proxy::socket_connection_proxy(socket_client_session *parent, connection *conn)
        : socket_subsession(parent)
        , conn_(conn)
        , progress_reported_(false) {
    }

    void socket_connection_proxy::dispatch(service::ipc_context *ctx) {
        if (parent_->is_oldarch()) {
            switch (ctx->msg->function) {
            default:
                LOG_ERROR(SERVICE_ESOCK, "Unimplemented socket connection opcode: {}", ctx->msg->function);
                ctx->complete(epoc::error_none);
                break;
            }
        } else {
            if (ctx->sys->get_symbian_version_use() >= epocver::epoc95) {
                switch (ctx->msg->function) {
                default:
                    LOG_ERROR(SERVICE_ESOCK, "Unimplemented socket connection opcode: {}", ctx->msg->function);
                    ctx->complete(epoc::error_none);
                    break;
                }
            } else {
                switch (ctx->msg->function) {
                case socket_cm_api_ext_interface_send_receive:
                    break;
                default:
                    LOG_ERROR(SERVICE_ESOCK, "Unimplemented socket connection opcode: {}", ctx->msg->function);
                    ctx->complete(epoc::error_none);
                    break;
                }
            }
        }
    }
}