#pragma once

#include <common/container.h>
#include <common/vecx.h>

#include <drivers/graphics/common.h>
#include <services/window/classes/config.h>
#include <services/window/common.h>

#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <vector>

namespace eka2l1 {
    namespace kernel {
        class chunk;
    }

    namespace config {
        struct app_setting;
    }

    class window_server;
    class ntimer;
}

namespace eka2l1::drivers {
    class graphics_driver;
    class graphics_command_builder;
}

namespace eka2l1::epoc {
    const std::uint32_t WORD_PALETTE_ENTRIES_COUNT = 16;

    struct window;
    struct window_group;
    struct screen;

    enum focus_change_property {
        focus_change_target,
        focus_change_name
    };

    using focus_change_callback_handler = std::function<void(void *, window_group *, focus_change_property)>;
    using screen_redraw_callback_handler = std::function<void(void *, screen *, bool)>;
    using screen_mode_change_callback_handler = std::function<void(void *, screen *, const int)>;

    struct screen {
        int number;
        int ui_rotation;

        std::uint8_t refresh_rate;

        float display_scale_factor;
        float logic_scale_factor_x;
        float logic_scale_factor_y;
        float requested_ui_scale_factor;

        std::unique_ptr<epoc::window> root;
        drivers::handle screen_texture;
        drivers::handle dsa_texture;

        epoc::display_mode disp_mode;

        std::uint64_t last_vsync;
        std::uint64_t last_fps_check;
        std::uint64_t last_fps;
        std::uint64_t frame_passed_per_sec;

        epoc::config::screen scr_config;
        std::uint8_t crr_mode;
        std::uint8_t physical_mode;

        epoc::window_group *focus;

        screen *next;

        kernel::chunk *screen_buffer_chunk;
        std::mutex screen_mutex;

        eka2l1::vec2 absolute_pos;

        std::map<std::int32_t, eka2l1::rect> pointer_areas_;
        eka2l1::vec2 pointer_cursor_pos_;

        std::uint32_t flags_ = 0;
        std::int32_t active_dsa_count_ = 0;

        bool sync_screen_buffer = false;

        enum {
            FLAG_NEED_RECALC_VISIBLE = 1 << 0,
            FLAG_ORIENTATION_LOCK = 1 << 1,
            FLAG_AUTO_CLEAR_BACKGROUND = 1 << 2,
            FLAG_SERVER_REDRAW_PENDING = 1 << 3,
            FLAG_CLIENT_REDRAW_PENDING = 1 << 4,
            FLAG_SCREEN_UPSCALE_FACTOR_LOCK = 1 << 5,
            FLAG_IS_SCREENPLAY = 1 << 6
        };

        using focus_change_callback = std::pair<void *, focus_change_callback_handler>;
        using screen_redraw_callback = std::pair<void *, screen_redraw_callback_handler>;
        using screen_mode_change_callback = std::pair<void *, screen_mode_change_callback_handler>;

        common::identity_container<focus_change_callback> focus_callbacks;
        common::identity_container<screen_redraw_callback> screen_redraw_callbacks;
        common::identity_container<screen_mode_change_callback> screen_mode_change_callbacks;

        void fire_focus_change_callbacks(const focus_change_property property);
        void fire_screen_redraw_callbacks(const bool is_dsa);
        void fire_screen_mode_change_callbacks(const int old_mode);

        std::size_t add_focus_change_callback(void *userdata, focus_change_callback_handler handler);
        bool remove_focus_change_callback(const std::size_t cb);

        std::size_t add_screen_redraw_callback(void *userdata, screen_redraw_callback_handler handler);
        bool remove_screen_redraw_callback(const std::size_t cb);

        std::size_t add_screen_mode_change_callback(void *userdata, screen_mode_change_callback_handler handler);
        bool remove_screen_mode_change_callback(const std::size_t cb);

        void vsync(ntimer *timing, std::uint64_t &next_vsync_us);

        explicit screen(const int number, epoc::config::screen &scr_conf);

        void abort_all_dsas(const std::int32_t reason);
        void recalculate_visible_regions(bool dont_trigger_redraw = false);

        void restore_from_config(drivers::graphics_driver *driver, const eka2l1::config::app_setting &setting);
        void try_change_display_rescale(drivers::graphics_driver *driver, const float scale_factor);

        void store_to_config(const eka2l1::config::app_setting &setting);

        epoc::window_group *get_group_chain();

        std::uint8_t *screen_buffer_ptr();
        eka2l1::vec2 size() const;
        const epoc::config::screen_mode &current_mode() const;
        const int total_screen_mode() const;
        const epoc::config::screen_mode *mode_info(const int number) const;

        void set_screen_mode(window_server *winserv, drivers::graphics_driver *drv, const int mode);
        void set_native_scale_factor(drivers::graphics_driver *driver, const float scale_factor_x,
            const float scale_factor_y);

        void resize(drivers::graphics_driver *driver, const eka2l1::vec2 &new_size);
        void deinit(drivers::graphics_driver *driver);
        bool redraw(drivers::graphics_command_builder &builder, const bool need_bind);
        void redraw(drivers::graphics_driver *driver);

        epoc::window_group *update_focus(window_server *serv, epoc::window_group *closing_group);

        const bool need_update_visible_regions() const {
            return flags_ & FLAG_NEED_RECALC_VISIBLE;
        }

        const bool auto_clear_enabled() const {
            return flags_ & FLAG_AUTO_CLEAR_BACKGROUND;
        }

        void set_client_draw_pending() {
            flags_ |= FLAG_CLIENT_REDRAW_PENDING;
        }

        void set_is_screenplay_architecture(bool is_screenplay) {
            if (!is_screenplay) {
                flags_ &= ~FLAG_IS_SCREENPLAY;
                return;
            }
            flags_ |= FLAG_IS_SCREENPLAY;
        }

        bool is_screenplay_architecture() const {
            return flags_ & FLAG_IS_SCREENPLAY;
        }

        void need_update_visible_regions(const bool value);

        void ref_dsa_usage();
        void deref_dsa_usage();
    };
}