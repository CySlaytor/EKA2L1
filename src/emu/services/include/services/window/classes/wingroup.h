#pragma once

#include <config/app_settings.h>
#include <services/window/classes/winbase.h>
#include <vector>

namespace eka2l1::kernel {
    class process;
}

namespace eka2l1::epoc {
    struct top_canvas;

    struct window_group : public epoc::window {
        std::u16string name;
        std::unique_ptr<top_canvas> top;

        eka2l1::config::app_setting saved_setting;
        std::size_t uid_owner_change_callback_handle;

        kernel::process *uid_owner_change_process;
        ws::uid screen_change_event_handle;

        bool can_receive_focus() { return flags & flag_focus_receiveable; }

        void set_receive_focus(const bool val) {
            flags &= ~flag_focus_receiveable;
            if (val)
                flags |= flag_focus_receiveable;
        }

        explicit window_group(window_server_client_ptr client, screen *scr, epoc::window *parent,
            const std::uint32_t client_handle);

        ~window_group() override;

        void on_owner_process_uid_type_change(const std::uint32_t new_uid);
        void set_name(const std::u16string &new_name);

        void receive_focus(service::ipc_context &context, ws_cmd &cmd);
        void set_name(service::ipc_context &context, ws_cmd &cmd);
        void destroy(service::ipc_context &context, ws_cmd &cmd);
        bool execute_command(service::ipc_context &context, ws_cmd &cmd) override;

        void lost_focus();
        void gain_focus();
    };
}