#include <services/socket/host.h>
#include <services/socket/server.h>
#include <system/epoc.h>
#include <utils/err.h>

namespace eka2l1::epoc::socket {
    socket_host_resolver::socket_host_resolver(socket_client_session *parent, std::unique_ptr<host_resolver> &resolver,
        connection *conn)
        : socket_subsession(parent)
        , resolver_(std::move(resolver))
        , conn_(conn) {
    }

    void socket_host_resolver::dispatch(service::ipc_context *ctx) {
        LOG_ERROR(SERVICE_ESOCK, "Unimplemented socket host resolver opcode: {}", ctx->msg->function);
        ctx->complete(epoc::error_not_supported);
    }
}