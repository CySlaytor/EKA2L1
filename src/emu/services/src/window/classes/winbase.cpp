#include <services/window/classes/winbase.h>
#include <services/window/op.h>
#include <services/window/opheader.h>
#include <services/window/screen.h>
#include <services/window/window.h>

#include <kernel/kernel.h>
#include <kernel/timing.h>
#include <utils/err.h>

namespace eka2l1::epoc {
    void window::queue_event(const epoc::event &evt) {
        client->queue_event(evt);
    }

    void window::set_parent(window *new_parent) {
        parent = new_parent;

        if (parent) {
            sibling = parent->child;
            parent->child = this;
        }
    }

    window *window::root_window() {
        return scr->root.get();
    }

    void window::walk_tree(window_tree_walker *walker, const window_tree_walk_style style) {
        window *end = root_window();
        window *cur = this;
        window *sibling = cur->sibling;
        window *parent = cur->parent;

        if (style != window_tree_walk_style::bonjour_previous_siblings) {
            if (style == window_tree_walk_style::bonjour_children) {
                end = this;
            }

            sibling = cur;

            while (sibling->child != nullptr) {
                sibling = sibling->child;
            }
        }

        do {
            if (sibling != nullptr) {
                cur = sibling;

                while (cur->child != nullptr) {
                    cur = cur->child;
                }
            } else {
                cur = parent;
            }

            if (cur == end) {
                break;
            }

            parent = cur->parent;
            sibling = cur->sibling;

            if (walker->do_it(cur)) {
                return;
            }
        } while (true);
    }

    window::~window() {
        while (child != nullptr) {
            child->parent = nullptr;
            child = child->sibling;
        }
    }

    void window::set_position(const int new_pos) {
        if (!parent) {
            return;
        }
        if (check_order_change(new_pos)) {
            move_window(parent, new_pos);

            if (type == epoc::window_kind::group) {
                window_server &serv = client->get_ws();
                scr->update_focus(&serv, nullptr);
            }
        }
    }

    void window::move_window(epoc::window *new_parent, const int new_pos) {
        remove_from_sibling_list();

        window **prev = &new_parent->child;

        while (*prev != nullptr && priority < (*prev)->priority) {
            prev = &((*prev)->sibling);
        }

        int pos = new_pos;

        while (pos-- != 0 && *prev != nullptr && (*prev)->priority == priority) {
            prev = &((*prev)->sibling);
        }

        epoc::window *old_parent = parent;

        sibling = *prev;
        parent = new_parent;
        *prev = this;

        if (type == window_kind::group || type == window_kind::client || new_parent != old_parent) {
            scr->need_update_visible_regions(true);
            client->get_ws().get_anim_scheduler()->schedule(client->get_ws().get_graphics_driver(),
                scr, client->get_ws().get_ntimer()->microseconds());
        }
    }

    bool window::check_order_change(const int new_pos) {
        window *cur = parent->child;
        window *prev = nullptr;

        while ((cur == this) || ((cur != nullptr) && (priority < cur->priority))) {
            prev = cur;
            cur = cur->sibling;
        }

        if (prev == this) {
            cur = this;
        } else if ((cur == nullptr) || ((cur->sibling == this) && (priority > cur->priority))) {
            return true;
        }

        int pos = new_pos;

        while ((pos-- != 0) && (cur->sibling != nullptr) && (priority == cur->sibling->priority)) {
            cur = cur->sibling;
        }

        return (cur != this);
    }

    void window::remove_from_sibling_list() {
        if (!parent) {
            return;
        }

        window *ite = parent->child;

        if (parent->child == this) {
            parent->child = sibling;
            return;
        }

        while (ite) {
            if (ite->sibling == this) {
                break;
            }

            ite = ite->sibling;
        }

        if (ite)
            ite->sibling = sibling;
    }

    int window::ordinal_position(const bool full) {
        window *win = parent->child;

        if (!full) {
            while (win->priority > priority) {
                win = win->sibling;
            }
        }

        int count = 0;

        for (count = 0; win != this; count++) {
            win = win->sibling;
        }

        return count;
    }

    bool window::execute_command_for_general_node(eka2l1::service::ipc_context &ctx, eka2l1::ws_cmd &cmd) {
        epoc::version cli_ver = client->client_version();
        kernel_system *kern = client->get_ws().get_kernel_system();

        if ((cli_ver.major == WS_MAJOR_VER) && (cli_ver.minor == WS_MINOR_VER)) {
            if ((cli_ver.build <= WS_OLDARCH_VER) || (kern->get_epoc_version() <= epocver::epoc80)) {
                if (cmd.header.op >= EWsWinOpAbsPosition) {
                    cmd.header.op += 1;
                }
            }

            if (cli_ver.build <= WS_NEWARCH_VER) {
                if ((cmd.header.op >= EWsWinOpSendAdvancedPointerEvent) && (kern->get_epoc_version() <= epocver::epoc94)) {
                    cmd.header.op += 1;
                }
            }
        }

        TWsWindowOpcodes op = static_cast<decltype(op)>(cmd.header.op);

        switch (op) {
        case EWsWinOpEnableModifierChangedEvents: {
            epoc::event_mod_notifier_user nof;
            nof.notifier = *reinterpret_cast<event_mod_notifier *>(cmd.data_ptr);
            nof.user = this;

            client->add_event_notifier<epoc::event_mod_notifier_user>(nof);
            ctx.complete(epoc::error_none);

            return true;
        }

        case EWsWinOpOrdinalPosition: {
            ctx.complete(ordinal_position(false));
            return true;
        }

        case EWsWinOpFullOrdinalPosition: {
            ctx.complete(ordinal_position(true));
            return true;
        }

        case EWsWinOpSetOrdinalPosition: {
            const int position = *reinterpret_cast<int *>(cmd.data_ptr);
            set_position(position);
            ctx.complete(epoc::error_none);

            return true;
        }

        case EWsWinOpSetOrdinalPositionPri: {
            ws_cmd_ordinal_pos_pri *info = reinterpret_cast<decltype(info)>(cmd.data_ptr);
            priority = info->pri1;
            const int position = info->pri2;

            set_position(position);
            ctx.complete(epoc::error_none);

            return true;
        }

        case EWsWinOpSetOrdinalPriorityAdjust: {
            priority = *reinterpret_cast<std::int32_t *>(cmd.data_ptr);
            set_position(0);

            ctx.complete(epoc::error_none);
            return true;
        }

        case EWsWinOpIdentifier: {
            ctx.complete(static_cast<int>(id));
            return true;
        }

        case EWsWinOpEnableErrorMessages: {
            epoc::event_control ctrl = *reinterpret_cast<epoc::event_control *>(cmd.data_ptr);
            epoc::event_error_msg_user nof;
            nof.when = ctrl;
            nof.user = this;

            ctx.complete(epoc::error_none);

            return true;
        }

        case EWsWinOpEnableFocusChangeEvents: {
            if (!focus_group_change_event_handle) {
                epoc::event_focus_group_change_user evt;
                evt.user = this;

                focus_group_change_event_handle = client->add_event_notifier<epoc::event_focus_group_change_user>(evt);
            }

            ctx.complete(epoc::error_none);
            return true;
        }

        case EWsWinOpClientHandle:
            ctx.complete(static_cast<int>(client_handle));
            return true;

        default: {
            break;
        }
        }

        return false;
    }
}