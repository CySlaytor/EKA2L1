#include <common/cvt.h>
#include <drivers/ui/input_dialog.h>
#include <services/notifier/notifier.h>
#include <services/notifier/queries.h>
#include <system/epoc.h>
#include <utils/consts.h>
#include <utils/err.h>

namespace eka2l1 {
    std::string get_notifier_server_name_by_epocver(const epocver ver) {
        if (ver < epocver::epoc7) {
            return "Notifier";
        }
        return "!Notifier";
    }

    notifier_server::notifier_server(eka2l1::system *sys)
        : service::typical_server(sys, get_notifier_server_name_by_epocver(sys->get_symbian_version_use())) {
        epoc::notifier::add_builtin_plugins(kern, plugins_);
    }

    epoc::notifier::plugin_base *notifier_server::get_plugin(const epoc::uid id) {
        auto result = std::lower_bound(plugins_.begin(), plugins_.end(), id, [](const epoc::notifier::plugin_instance &lhs, const epoc::uid rhs) {
            return lhs->unique_id() < rhs;
        });

        if ((result == plugins_.end()) || ((*result)->unique_id() != id)) {
            return nullptr;
        }
        return result->get();
    }

    void notifier_server::connect(service::ipc_context &context) {
        create_session<notifier_client_session>(&context);
        context.complete(epoc::error_none);
    }

    notifier_client_session::notifier_client_session(service::typical_server *serv, const kernel::uid ss_id,
        epoc::version client_version)
        : service::typical_session(serv, ss_id, client_version) {
    }

    void notifier_client_session::start_notifier(service::ipc_context *ctx) {
        std::optional<epoc::uid> plugin_uid = ctx->get_argument_value<epoc::uid>(0);
        if (!plugin_uid) {
            ctx->complete(epoc::error_argument);
            return;
        }

        epoc::notifier::plugin_base *plug = server<notifier_server>()->get_plugin(plugin_uid.value());
        if (!plug) {
            LOG_ERROR(SERVICE_NOTIFIER, "Can't find the plugin with UID 0x{:X}. This is fine (but take note).", plugin_uid.value());
            ctx->complete(epoc::error_none);
            return;
        }

        kernel::process *caller_pr = ctx->msg->own_thr->owning_process();

        epoc::desc8 *request_data = eka2l1::ptr<epoc::desc8>(ctx->msg->args.args[1]).get(caller_pr);
        epoc::des8 *respond_data = eka2l1::ptr<epoc::des8>(ctx->msg->args.args[2]).get(caller_pr);

        if (!request_data) {
            ctx->complete(epoc::error_argument);
            return;
        }

        epoc::notify_info complete_info;
        complete_info.sts = ctx->msg->request_sts;
        complete_info.requester = ctx->msg->own_thr;

        plug->handle(request_data, respond_data, complete_info);
    }

    void notifier_client_session::fetch(service::ipc_context *ctx) {
        switch (ctx->msg->function) {
        case notifier_start:
        case notifier_update:
        case notfiier_start_and_get_response:
            start_notifier(ctx);
            break;

        case notifier_cancel:
            ctx->complete(epoc::error_none);
            break;

        default:
            LOG_ERROR(SERVICE_NOTIFIER, "Unimplemented opcode for Notifier server 0x{:X}", ctx->msg->function);
            break;
        }
    }
}