#pragma once

#include <common/linked.h>
#include <common/vecx.h>
#include <memory>
#include <services/window/classes/wsobj.h>
#include <services/window/common.h>

namespace eka2l1::epoc {
    struct window;

    enum class window_kind {
        normal,
        group,
        top_client,
        client
    };

    enum class window_tree_walk_style {
        bonjour_previous_siblings,
        bonjour_children,
        bonjour_children_and_previous_siblings
    };

    struct window_tree_walker {
        virtual bool do_it(window *win) = 0;
    };

    struct window : public window_client_obj {
        window *parent{ nullptr };
        window *sibling{ nullptr };
        window *child{ nullptr };

        std::int32_t priority{ 0 };
        std::uint32_t client_handle{ 0 };

        ws::uid focus_group_change_event_handle{ 0 };
        window_kind type;

        enum {
            flags_shadow_disable = 1 << 0,
            flags_active = 1 << 1,
            flags_visible = 1 << 2,
            flags_allow_pointer_grab = 1 << 3,
            flags_non_fading = 1 << 4,
            flags_enable_alpha = 1 << 5,
            flags_faded = 1 << 6,
            flags_faded_default_param = 1 << 7,
            flags_faded_also_children = 1 << 8,
            flags_dsa = 1 << 9,
            flags_enable_pbe = 1 << 10,
            flags_in_redraw = 1 << 11,
            flag_focus_receiveable = 1 << 12,
            flag_winmode_fixed = 1 << 13,
            flag_visiblity_event_report = 1 << 14,
            flag_content_changed = 1 << 16,
            flag_shape_region = 1 << 17,
            flag_fix_native_orientation = 1 << 18
        };

        std::uint32_t flags;

        std::uint8_t black_map = 128;
        std::uint8_t white_map = 255;

        std::uint32_t get_client_handle() const { return client_handle; }
        void set_client_handle(const std::uint32_t new_handle) { client_handle = new_handle; }

        window *root_window();
        int ordinal_position(const bool full);

        bool execute_command_for_general_node(eka2l1::service::ipc_context &ctx, eka2l1::ws_cmd &cmd);
        virtual void queue_event(const epoc::event &evt);
        void walk_tree(window_tree_walker *walker, const window_tree_walk_style style);
        void move_window(epoc::window *new_parent, const int new_pos);
        void set_position(const int new_pos);
        bool check_order_change(const int new_pos);
        void remove_from_sibling_list();
        void set_parent(window *parent);

        explicit window(window_server_client_ptr client, screen *scr, window *parent)
            : window_client_obj(client, scr)
            , type(window_kind::normal)
            , parent(nullptr)
            , sibling(nullptr)
            , child(nullptr)
            , flags(flags_visible) {
            set_parent(parent);
        }

        explicit window(window_server_client_ptr client, screen *scr, window *parent, window_kind type)
            : window_client_obj(client, scr)
            , type(type)
            , parent(nullptr)
            , sibling(nullptr)
            , child(nullptr)
            , flags(flags_visible) {
            set_parent(parent);
        }

        void set_initial_state() {
            set_position(0);
            if (parent && ((parent->flags & flags_visible) == 0)) {
                flags &= ~flags_visible;
            }
        }

        virtual ~window() override;
    };
}