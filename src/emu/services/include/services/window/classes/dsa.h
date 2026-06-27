#pragma once

#include <common/region.h>
#include <services/window/classes/wsobj.h>
#include <utils/reqsts.h>

#include <queue>
#include <string>

namespace eka2l1::kernel {
    class msg_queue;
    class thread;
}

namespace eka2l1::epoc {
    struct screen;
    struct canvas_base;

    struct dsa : public window_client_obj {
        canvas_base *husband_;
        kernel::msg_queue *dsa_must_abort_queue_;
        kernel::msg_queue *dsa_complete_queue_;

        kernel::thread *sync_thread_;
        eka2l1::ptr<epoc::request_status> sync_status_;

        epoc::notify_info dsa_must_stop_notify_;
        common::region operate_region_;

        enum state {
            state_none,
            state_prepare,
            state_running,
            state_completed
        } state_;

        explicit dsa(window_server_client_ptr client);
        ~dsa() override;

        void visible_region_changed(const common::region &new_region);

        void do_cancel();
        void abort(const std::int32_t reason);

        void request_access(eka2l1::service::ipc_context &ctx, eka2l1::ws_cmd &cmd);
        void get_region(eka2l1::service::ipc_context &ctx, eka2l1::ws_cmd &cmd);
        void cancel(eka2l1::service::ipc_context &ctx, eka2l1::ws_cmd &cmd);
        void destroy(eka2l1::service::ipc_context &context, eka2l1::ws_cmd &cmd);

        bool execute_command(eka2l1::service::ipc_context &ctx, eka2l1::ws_cmd &cmd) override;
    };
}