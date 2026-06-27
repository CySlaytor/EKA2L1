#include <services/internet/connmonitor.h>
#include <system/epoc.h>
#include <utils/err.h>

namespace eka2l1 {
    connmonitor_server::connmonitor_server(eka2l1::system *sys)
        : service::typical_server(sys, "!ConnectionMonitorServer") {
    }

    void connmonitor_server::connect(service::ipc_context &context) {
        create_session<connmonitor_client_session>(&context);
        context.complete(epoc::error_none);
    }

    connmonitor_client_session::connmonitor_client_session(service::typical_server *serv, const kernel::uid ss_id,
        epoc::version client_version)
        : service::typical_session(serv, ss_id, client_version) {
    }

    void connmonitor_client_session::fetch(service::ipc_context *ctx) {
        switch (ctx->msg->function) {
        case connmonitor_get_connection_count: {
            get_connection_count(ctx);
            break;
        }

        case connmonitor_get_connection_info: {
            get_connection_info(ctx);
            break;
        }

        case connmonitor_get_int_attribute: {
            get_int_attribute(ctx);
            break;
        }

        case connmonitor_get_uint_attribute: {
            get_uint_attribute(ctx);
            break;
        }

        case connmonitor_receive_event: {
            receive_event(ctx);
            break;
        }

        case connmonitor_cancel_receive_event: {
            cancel_receive_event(ctx);
            break;
        }

        default: {
            LOG_ERROR(SERVICE_INTERNET, "Unimplemented opcode for ConnectionMonitorServer 0x{:X}", ctx->msg->function);
            break;
        }
        }
    }

    void connmonitor_client_session::get_connection_count(eka2l1::service::ipc_context *ctx) {
        std::uint32_t connection_count = 1;
        ctx->write_data_to_descriptor_argument(0, connection_count);
        ctx->complete(epoc::error_none);
    }

    void connmonitor_client_session::receive_event(eka2l1::service::ipc_context *ctx) {
        nof_info = epoc::notify_info(ctx->msg->request_sts, ctx->msg->own_thr);
    }

    void connmonitor_client_session::get_connection_info(eka2l1::service::ipc_context *ctx) {
        std::uint32_t index = *(ctx->get_argument_value<std::uint32_t>(0));
        std::uint32_t *connection_id_ptr = reinterpret_cast<std::uint32_t *>(ctx->get_descriptor_argument_ptr(1));
        std::uint32_t *subconnection_count_ptr = reinterpret_cast<std::uint32_t *>(ctx->get_descriptor_argument_ptr(2));

        *connection_id_ptr = bearer_id_gprs;
        *subconnection_count_ptr = 1;

        ctx->complete(epoc::error_none);
    }

    void connmonitor_client_session::get_int_attribute(eka2l1::service::ipc_context *ctx) {
        std::int32_t *value_ptr = reinterpret_cast<std::int32_t *>(ctx->get_descriptor_argument_ptr(3));
        *value_ptr = 0;
        ctx->complete(epoc::error_none);
    }

    void connmonitor_client_session::get_uint_attribute(eka2l1::service::ipc_context *ctx) {
        std::uint32_t *value_ptr = reinterpret_cast<std::uint32_t *>(ctx->get_descriptor_argument_ptr(3));
        *value_ptr = 0;
        ctx->complete(epoc::error_none);
    }

    void connmonitor_client_session::cancel_receive_event(eka2l1::service::ipc_context *ctx) {
        nof_info.complete(epoc::error_cancel);
        ctx->complete(epoc::error_none);
    }
}