#pragma once

#include <memory>
#include <services/window/classes/wsobj.h>
#include <services/window/common.h>

namespace eka2l1::epoc {
    struct screen;

    struct screen_device : public window_client_obj {
    private:
        std::int32_t local_screen_mode_;

    public:
        bool execute_command(eka2l1::service::ipc_context &ctx, eka2l1::ws_cmd &cmd) override;
        void set_screen_mode_and_rotation(eka2l1::service::ipc_context &ctx, eka2l1::ws_cmd &cmd);
        void get_screen_size_mode_and_rotation(eka2l1::service::ipc_context &ctx, eka2l1::ws_cmd &cmd, const bool bonus_the_twips);
        void get_default_screen_size_and_rotation(eka2l1::service::ipc_context &ctx, eka2l1::ws_cmd &cmd, const bool twips);
        void get_current_screen_mode_scale(eka2l1::service::ipc_context &ctx, eka2l1::ws_cmd &cmd);
        void get_default_screen_mode_origin(eka2l1::service::ipc_context &ctx, eka2l1::ws_cmd &cmd);
        void set_app_screen_mode(eka2l1::service::ipc_context &ctx, eka2l1::ws_cmd &cmd);

        explicit screen_device(window_server_client_ptr client, epoc::screen *scr);
    };
}