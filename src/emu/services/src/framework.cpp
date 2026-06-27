#include <common/log.h>
#include <config/config.h>
#include <services/framework.h>
#include <services/utils.h>
#include <system/epoc.h>

namespace eka2l1::service {
    normal_object_container::~normal_object_container() {
        clear();
    }

    void normal_object_container::clear() {
        const std::lock_guard<std::recursive_mutex> guard(obj_lock);
        objs.clear();
    }

    bool normal_object_container::remove(epoc::ref_count_object *obj) {
        const std::lock_guard<std::recursive_mutex> guard(obj_lock);
        auto res = std::lower_bound(objs.begin(), objs.end(), obj,
            [](const ref_count_object_heap_ptr &lhs, const epoc::ref_count_object *rhs) {
                return lhs->id < rhs->id;
            });

        if (res == objs.end() || (res->get()->id != obj->id)) {
            return false;
        }

        ref_count_object_heap_ptr moved = std::move(*res);
        objs.erase(res);
        moved.reset();

        return true;
    }

    typical_server::typical_server(system *sys, const std::string name)
        : server(sys->get_kernel_system(), sys, nullptr, name, true, false) {
    }

    typical_server::~typical_server() {
        sessions.clear();
    }

    void typical_server::disconnect_impl(service::session *ss) {
        if (!ss) {
            return;
        }
        sessions.erase(ss->unique_id());
    }

    void typical_server::disconnect(service::ipc_context &ctx) {
        disconnect_impl(ctx.msg->msg_session);
        ctx.complete(0);
    }

    std::optional<epoc::version> typical_server::get_version(service::ipc_context *ctx) {
        kernel_system *kern = ctx->sys->get_kernel_system();
        std::optional<epoc::version> ver = get_server_version(kern, ctx);
        return ver;
    }

    void typical_server::process_accepted_msg() {
        ipc_msg_ptr process_msg = nullptr;
        receive(process_msg);

        if (!process_msg) {
            return;
        }

        ipc_context context;
        context.sys = sys;
        context.msg = process_msg;

        auto func = ipc_funcs.find(process_msg->function);
        if (func != ipc_funcs.end()) {
            func->second.wrapper(context);
            return;
        }

        auto ss_ite = sessions.find(process_msg->msg_session->unique_id());
        if (ss_ite == sessions.end()) {
            LOG_TRACE(SERVICE_TRACK, "Can't find responsible server-side session to client session with ID {}",
                process_msg->msg_session->unique_id());
            return;
        }

        config::state *conf = sys->get_config();
        if (conf->log_ipc) {
            LOG_INFO(SERVICE_TRACK, "Calling service: {}, id: {}", raw_name(), process_msg->function);
        }

        ss_ite->second->fetch(&context);
    }
}