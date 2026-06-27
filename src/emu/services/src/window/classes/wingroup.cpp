#include <services/window/classes/scrdvc.h>
#include <services/window/classes/wingroup.h>
#include <services/window/classes/winuser.h>
#include <services/window/op.h>
#include <services/window/opheader.h>
#include <services/window/window.h>
#include <utils/sec.h>

#include <common/cvt.h>
#include <common/log.h>

#include <config/app_settings.h>
#include <kernel/kernel.h>

#include <utils/err.h>

namespace eka2l1::epoc {
    static epoc::security_policy key_capture_policy({ epoc::cap_sw_event });

    void window_group::lost_focus() {
        queue_event(epoc::event{ client_handle, epoc::event_code::focus_lost });
    }

    void window_group::gain_focus() {
        queue_event(epoc::event{ client_handle, epoc::event_code::focus_gained });
    }

    void window_group::receive_focus(service::ipc_context &context, ws_cmd &cmd) {
        flags &= ~flag_focus_receiveable;

        if (*reinterpret_cast<std::uint32_t *>(cmd.data_ptr)) {
            flags |= flag_focus_receiveable;

            LOG_TRACE(SERVICE_WINDOW, "Request group {} to enable keyboard focus", common::ucs2_to_utf8(name));
        } else {
            LOG_TRACE(SERVICE_WINDOW, "Request group {} to disable keyboard focus", common::ucs2_to_utf8(name));
        }

        scr->update_focus(&client->get_ws(), nullptr);
        context.complete(epoc::error_none);
    }

    void window_group::on_owner_process_uid_type_change(const std::uint32_t new_uid) {
        kernel_system *kern = client->get_ws().get_kernel_system();

        eka2l1::config::app_settings *settings = kern->get_app_settings();
        eka2l1::config::app_setting *the_setting = settings->get_setting(new_uid);

        if (!the_setting) {
            return;
        }

        saved_setting = *the_setting;

        if (this == scr->focus) {
            scr->restore_from_config(client->get_ws().get_graphics_driver(), saved_setting);
        }
    }

    static void window_group_process_uid_type_change_callback(void *userdata, const kernel::process_uid_type &type) {
        epoc::window_group *group = reinterpret_cast<epoc::window_group *>(userdata);

        if (group) {
            group->on_owner_process_uid_type_change(std::get<2>(type));
        }
    }

    window_group::window_group(window_server_client_ptr client, screen *scr, epoc::window *parent, const std::uint32_t client_handle)
        : window(client, scr, parent, window_kind::group)
        , uid_owner_change_callback_handle(0)
        , uid_owner_change_process(nullptr)
        , screen_change_event_handle(0) {
        set_initial_state();

        top = std::make_unique<top_canvas>(client, scr, this);
        child = top.get();

        set_client_handle(client_handle);

        kernel::process *current = client->get_client()->owning_process();
        kernel::process *mama = current->get_final_setting_process();

        on_owner_process_uid_type_change(mama->get_uid());
        uid_owner_change_callback_handle = mama->register_uid_type_change_callback(this,
            window_group_process_uid_type_change_callback);

        uid_owner_change_process = mama;
    }

    window_group::~window_group() {
        if (uid_owner_change_process) {
            uid_owner_change_process->unregister_uid_type_change_callback(uid_owner_change_callback_handle);
        }

        remove_from_sibling_list();

        if (this == scr->focus) {
            set_receive_focus(false);
            scr->update_focus(&client->get_ws(), this);
        }

        if (scr) {
            scr->need_update_visible_regions(true);
        }
    }

    void window_group::set_name(const std::u16string &new_name) {
        name = std::move(new_name);

        if (this == scr->focus) {
            scr->fire_focus_change_callbacks(focus_change_name);
        }
    }

    void window_group::set_name(service::ipc_context &context, ws_cmd &cmd) {
        std::optional<std::u16string> name_re;

        if (cmd.header.cmd_len > 4) {
            struct ws_cmd_header_set_name_legacy {
                std::uint32_t len;
                eka2l1::ptr<epoc::desc16> name_to_set;
            };

            kernel::process *requester = context.msg->own_thr->owning_process();
            epoc::desc16 *des = reinterpret_cast<ws_cmd_header_set_name_legacy *>(cmd.data_ptr)->name_to_set.get(requester);

            name_re = des->to_std_string(requester);
        } else {
            name_re = context.get_argument_value<std::u16string>(remote_slot);
        }

        if (!name_re) {
            context.complete(epoc::error_argument);
            return;
        }

        set_name(name_re.value());
        context.complete(epoc::error_none);
    }

    void window_group::destroy(service::ipc_context &context, ws_cmd &cmd) {
        on_command_batch_done(context);

        context.complete(epoc::error_none);
        client->delete_object(cmd.obj_handle);
    }

    bool window_group::execute_command(service::ipc_context &ctx, ws_cmd &cmd) {
        bool result = execute_command_for_general_node(ctx, cmd);
        bool need_free = false;

        if (result) {
            return false;
        }

        TWsWindowOpcodes op = static_cast<decltype(op)>(cmd.header.op);

        switch (op) {
        case EWsWinOpFree:
            destroy(ctx, cmd);
            need_free = true;

            break;

        case EWsWinOpEnableScreenChangeEvents: {
            if (!screen_change_event_handle) {
                epoc::event_screen_change_user evt;
                evt.user = this;

                screen_change_event_handle = client->add_event_notifier<epoc::event_screen_change_user>(evt);
            }

            ctx.complete(epoc::error_none);
            break;
        }

        case EWsWinOpCaptureKey:
        case EWsWinOpCaptureKeyUpsAndDowns: {
            if (!ctx.satisfy(key_capture_policy)) {
                ctx.complete(epoc::error_permission_denied);
                break;
            }

            ws_cmd_capture_key *capture_key_cmd = reinterpret_cast<decltype(capture_key_cmd)>(cmd.data_ptr);

            epoc::event_capture_key_notifier capture_key_notify;

            capture_key_notify.user = this;
            capture_key_notify.keycode_ = capture_key_cmd->key;
            capture_key_notify.modifiers_ = capture_key_cmd->modifiers;
            capture_key_notify.modifiers_mask_ = capture_key_cmd->modifier_mask;
            capture_key_notify.type_ = (op == EWsWinOpCaptureKeyUpsAndDowns) ? epoc::event_key_capture_type::up_and_downs : epoc::event_key_capture_type::normal;
            capture_key_notify.pri_ = capture_key_cmd->priority;

            ctx.complete(client->add_event_notifier(capture_key_notify));

            break;
        }

        case EWsWinOpSetName: {
            set_name(ctx, cmd);
            break;
        }

        case EWsWinOpName: {
            ctx.write_arg(reply_slot, name);
            ctx.complete(epoc::error_none);
            break;
        }

        case EWsWinOpReceiveFocus: {
            receive_focus(ctx, cmd);
            break;
        }

        case EWsWinOpOrdinalPosition: {
            ctx.complete(ordinal_position(true));
            break;
        }

        case EWsWinOpOrdinalPriority: {
            ctx.complete(priority);
            break;
        }

        default: {
            LOG_ERROR(SERVICE_WINDOW, "Unimplemented window group opcode 0x{:X}!", cmd.header.op);
            ctx.complete(epoc::error_none);

            break;
        }
        }

        return need_free;
    }
}