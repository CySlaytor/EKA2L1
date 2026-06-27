#pragma once

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <unordered_map>

#include <common/e32inc.h>
#include <common/types.h>
#include <common/vecx.h>

#include <drivers/graphics/emu_window.h>
#include <services/window/keys.h>

#include <utils/consts.h>
#include <utils/des.h>

enum {
    cmd_slot = 0,
    reply_slot = 1,
    remote_slot = 2
};

namespace eka2l1::epoc {
    enum {
        base_handle = 0x40000000
    };

    enum class graphics_orientation {
        normal,
        rotated90,
        rotated180,
        rotated270
    };

    enum class display_mode {
        none,
        gray2,
        gray4,
        gray16,
        gray256,
        color16,
        color256,
        color64k,
        color16m,
        rgb,
        color4k,
        color16mu,
        color16ma,
        color16map,
        color_last
    };

    int get_num_colors_from_display_mode(const display_mode disp_mode);
    bool is_display_mode_color(const display_mode disp_mode);
    bool is_display_mode_mono(const display_mode disp_mode);
    bool is_display_mode_alpha(const display_mode disp_mode);
    int get_bpp_from_display_mode(const epoc::display_mode bpp);
    int get_byte_width(const std::uint32_t pixels_width, const std::uint8_t bits_per_pixel);
    epoc::display_mode string_to_display_mode(const std::string &disp_str);
    epoc::display_mode get_display_mode_from_bpp(const int bpp, const bool has_color);

    enum class pointer_cursor_mode {
        none,
        fixed,
        normal,
        window,
    };

    enum class window_type {
        redraw,
        backed_up,
        blank
    };

    enum event_modifier {
        event_modifier_repeatable = 0x001,
        event_modifier_keypad = 0x002,
        event_modifier_left_alt = 0x004,
        event_modifier_right_alt = 0x008,
        event_modifier_alt = 0x010,
        event_modifier_left_ctrl = 0x020,
        event_modifier_right_ctrl = 0x040,
        event_modifier_ctrl = 0x080,
        event_modifier_left_shift = 0x100,
        event_modifier_right_shift = 0x200,
        event_modifier_shift = 0x400,
        event_modifier_left_func = 0x800,
        event_modifier_right_func = 0x1000,
        event_modifier_func = 0x2000,
        event_modifier_caps_lock = 0x4000,
        event_modifier_num_lock = 0x8000,
        event_modifier_scroll_lock = 0x10000,
        event_modifier_key_up = 0x20000,
        event_modifier_special = 0x40000,
        event_modifier_double_click = 0x80000,
        event_modifier_pure_key_code = 0x100000,
        event_modifier_cancel_rot = 0x200000,
        event_modifier_no_rot = 0x0,
        event_modifier_rotate90 = 0x400000,
        event_modifier_rotate180 = 0x800000,
        event_modifier_rotate270 = 0x1000000,
        event_modifier_adv_pointer = 0x10000000,
        event_modifier_all_mods = 0x1FFFFFFF
    };

    enum class event_type {
        button1down,
        button1up,
        button2down,
        button2up,
        button3down,
        button3up,
        drag,
        move,
        button_repeat,
        switch_on,
        out_of_range,
        enter_close_proximity,
        exit_close_proximity,
        enter_high_pressure,
        exit_high_pressure,
        null_type = -1
    };

    enum class event_code {
        null,
        key,
        key_up,
        key_down,
        modifier_change,
        touch,
        touch_enter,
        touch_exit,
        event_pointer_buffer_ready,
        drag_and_drop,
        focus_lost,
        focus_gained,
        switch_on,
        event_password,
        window_groups_changed,
        event_error_msg,
        event_messages_ready,
        invalid,
        switch_off,
        key_switch_off,
        screen_change,
        focus_group_changed,
        case_opened,
        case_closed,
        group_list_change,
        window_visibility_change,

#ifdef SYMBIAN_PROCESS_MONITORING_AND_STARTUP
        restart_system,
#endif

        display_changed = window_visibility_change + 2,
        key_repeat = 100,
        group_win_open,
        group_win_close,
        win_close,
        direct_access_begin = 200,
        direct_access_end,
        heartbeat_timer_changed,
        power_mgmt = 900,
        reserved = 910,
        user = 1000,
    };

    struct key_event {
        std::uint32_t code;
        std::int32_t scancode;
        std::uint32_t modifiers;
        std::int32_t repeats;
    };

    struct pointer_event {
        event_type evtype;
        std::uint32_t modifier = 0;
        eka2l1::vec2 pos;
        eka2l1::vec2 parent_pos;
    };

    struct message_ready_event {
        std::int32_t window_group_id;
        epoc::uid message_uid;
        std::int32_t message_parameters_size;
    };

    struct window_visiblity_changed_event {
        enum {
            partially_visible = 1 << 0,
            not_visible = 1 << 1,
            fully_visible = 1 << 2
        };

        std::uint32_t flags_;
    };

    struct adv_pointer_event : public pointer_event {
        int pos_z;
        std::uint8_t ptr_num;
        std::uint8_t padding[3];
    };

    enum pointer_filter_type {
        pointer_none = 0x00,
        pointer_enter = 0x01,
        pointer_move = 0x02,
        pointer_drag = 0x04,
        pointer_simulated_event = 0x08,
        all = pointer_move | pointer_simulated_event
    };

    enum class text_alignment {
        left,
        center,
        right
    };

    enum class event_control {
        always,
        only_with_keyboard_focus,
        only_when_visible
    };

    struct event {
        event_code type;
        std::uint32_t handle;
        std::uint64_t time;

        union {
            adv_pointer_event adv_pointer_evt_;
            key_event key_evt_;
            message_ready_event msg_ready_evt_;
            window_visiblity_changed_event win_visibility_change_evt_;
        };

        event() {}
        event(const std::uint32_t handle, event_code evt_code);

        void operator=(const event &rhs) {
            type = rhs.type;
            handle = rhs.handle;
            time = rhs.time;

            adv_pointer_evt_ = rhs.adv_pointer_evt_;
        }
    };

    struct redraw_event {
        std::uint32_t handle;
        vec2 top_left;
        vec2 bottom_right;
    };

    static_assert(sizeof(redraw_event) == 20);

    constexpr key_code keymap[] = {
        key_null,
        key_backspace,
        key_tab,
        key_enter,
        key_escape,
        key_space,
        key_print_screen,
        key_pause,
        key_home,
        key_end,
        key_page_up,
        key_page_down,
        key_insert,
        key_delete,
        key_left_arrow,
        key_right_arrow,
        key_up_arrow,
        key_down_arrow,
        key_left_shift,
        key_right_shift,
        key_left_alt,
        key_right_alt,
        key_left_ctrl,
        key_right_ctrl,
        key_left_func,
        key_right_func,
        key_caps_lock,
        key_num_lock,
        key_scroll_lock,
        key_f1,
        key_f2,
        key_f3,
        key_f4,
        key_f5,
        key_f6,
        key_f7,
        key_f8,
        key_f9,
        key_f10,
        key_f11,
        key_f12,
        key_f13,
        key_f14,
        key_f15,
        key_f16,
        key_f17,
        key_f18,
        key_f19,
        key_f20,
        key_f21,
        key_f22,
        key_f23,
        key_f24,
        key_null,
        key_null,
        key_null,
        key_null,
        key_null,
        key_null,
        key_null,
        key_null,
        key_null,
        key_null,
        key_null,
        key_null,
        key_null,
        key_null,
        key_null,
        key_null,
        key_null,
        key_null,
        key_null,
        key_null,
        key_null,
        key_null,
        key_null,
        key_null,
        key_null,
        key_null,
        key_null,
        key_null,
        key_menu,
        key_backlight_on,
        key_backlight_off,
        key_backlight_toggle,
        key_inc_contrast,
        key_dec_contrast,
        key_slider_down,
        key_slider_up,
        key_dictaphone_play,
        key_dictaphone_stop,
        key_dictaphone_record,
        key_help,
        key_off,
        key_dial,
        key_inc_volume,
        key_dec_volume,
        key_device_0,
        key_device_1,
        key_device_2,
        key_device_3,
        key_device_4,
        key_device_5,
        key_device_6,
        key_device_7,
        key_device_8,
        key_device_9,
        key_device_a,
        key_device_b,
        key_device_c,
        key_device_d,
        key_device_e,
        key_device_f,
        key_application_0,
        key_application_1,
        key_application_2,
        key_application_3,
        key_application_4,
        key_application_5,
        key_application_6,
        key_application_7,
        key_application_8,
        key_application_9,
        key_application_a,
        key_application_b,
        key_application_c,
        key_application_d,
        key_application_e,
        key_application_f,
        key_yes,
        key_no,
        key_inc_brightness,
        key_dec_brightness,
        key_keyboard_extend,
        key_device_10,
        key_device_11,
        key_device_12,
        key_device_13,
        key_device_14,
        key_device_15,
        key_device_16,
        key_device_17,
        key_device_18,
        key_device_19,
        key_device_1a,
        key_device_1b,
        key_device_1c,
        key_device_1d,
        key_device_1e,
        key_device_1f,
        key_application_10,
        key_application_11,
        key_application_12,
        key_application_13,
        key_application_14,
        key_application_15,
        key_application_16,
        key_application_17,
        key_application_18,
        key_application_19,
        key_application_1a,
        key_application_1b,
        key_application_1c,
        key_application_1d,
        key_application_1e,
        key_application_1f,
        key_device_20,
        key_device_21,
        key_device_22,
        key_device_23,
        key_device_24,
        key_device_25,
        key_device_26,
        key_device_27,
        key_application_20,
        key_application_21,
        key_application_22,
        key_application_23,
        key_application_24,
        key_application_25,
        key_application_26,
        key_application_27
    };

    enum dsa_terminate_reason {
        dsa_terminate_cancel = 0,
        dsa_terminate_region = 1,
        dsa_terminate_screen_display_mode_change = 2,
        dsa_terminate_rotation_change = 3
    };

    static constexpr std::uint8_t WS_MAJOR_VER = 1;
    static constexpr std::uint8_t WS_MINOR_VER = 0;
    static constexpr std::uint16_t WS_OLDARCH_VER = 139;
    static constexpr std::uint16_t WS_NEWARCH_VER = 151;
    static constexpr std::uint64_t WS_DEFAULT_KEYBOARD_REPEAT_INIT_DELAY = 300000;
    static constexpr std::uint64_t WS_DEFAULT_KEYBOARD_REPEAT_NEXT_DELAY = 100000;
    static constexpr std::uint64_t WS_DELIVER_REPORT_VISIBILITY_INIT_DELAY = 400000;

    key_code map_scancode_to_keycode(std_scan_code scan_code);
    std_scan_code post_processing_scancode(std_scan_code input_code, int ui_rotation);

    static constexpr std::uint32_t KEYBIND_TYPE_MOUSE_CODE_BASE = 0x10000000;

    typedef std::map<std::pair<int, int>, std::uint32_t> button_map;
    typedef std::map<std::uint32_t, std::uint32_t> key_map;

    std::optional<std::uint32_t> map_button_to_inputcode(button_map &map, int controller_id, int button);
    std::optional<std::uint32_t> map_key_to_inputcode(key_map &map, std::uint32_t keycode);

    int get_approximate_pixel_to_twips_mul(const epocver ver);
}