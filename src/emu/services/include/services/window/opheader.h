#pragma once

#include <mem/ptr.h>
#include <services/window/common.h>
#include <utils/des.h>

#include <common/uid.h>
#include <common/vecx.h>

namespace eka2l1 {
    struct ws_cmd_header {
        uint16_t op;
        uint16_t cmd_len;
    };

    struct ws_cmd {
        ws_cmd_header header;
        uint32_t obj_handle;
        void *data_ptr;
    };

    struct ws_cmd_screen_device_header {
        int num_screen;
        uint32_t screen_dvc_ptr;
    };

    struct ws_cmd_window_header {
        int parent;
        uint32_t client_handle;
        epoc::window_type win_type;
        epoc::display_mode dmode;
    };

    struct ws_cmd_window_group_header {
        uint32_t client_handle;
        std::int32_t focus;
        uint32_t parent_id;
        uint32_t screen_device_handle;
    };

    struct ws_cmd_create_sprite_header {
        int window_handle;
        eka2l1::vec2 base_pos;
        int flags;
    };

    struct ws_cmd_ordinal_pos_pri {
        int pri2;
        int pri1;
    };

    struct ws_cmd_find_window_group_identifier {
        std::uint32_t previous_id;
        int offset;
        int length;
    };

    struct ws_cmd_find_window_group_identifier_thread {
        std::uint32_t previous_id;
        std::uint64_t thread_id;
    };

    struct ws_cmd_find_window_group_identifier_thread_eka1 {
        std::uint32_t previous_id;
        std::uint32_t thread_id;
    };

    struct ws_cmd_set_extent {
        eka2l1::vec2 pos;
        eka2l1::vec2 size;
    };

    struct ws_cmd_pointer_filter {
        std::uint32_t mask;
        std::uint32_t flags;
    };

    struct ws_cmd_send_event_to_window_group {
        int id;
        epoc::event evt;
    };

    struct ws_cmd_send_message_to_window_group {
        std::int32_t id_or_priority;
        epoc::uid uid;
        std::int32_t data_length;
        eka2l1::ptr<epoc::desc8> data;
    };

    struct ws_cmd_get_window_group_name_from_id {
        std::uint32_t id;
        int max_len;
    };

    struct ws_cmd_capture_key {
        std::uint32_t modifiers;
        std::uint32_t modifier_mask;
        std::uint32_t key;
        int priority;
    };

    struct ws_cmd_window_group_list {
        std::int32_t priority;
        std::int32_t count;
        std::int32_t screen_num;
    };

    struct ws_cmd_set_window_group_ordinal_position {
        std::uint32_t identifier;
        std::int32_t ord_pos;
    };

    struct ws_cmd_set_pointer_cursor_area {
        std::int32_t mode;
        eka2l1::rect pointer_area;
    };

    struct ws_cmd_keyboard_repeat_rate {
        std::uint32_t initial_time;
        std::uint32_t next_time;
    };
}