#pragma once

#include <common/bitfield.h>
#include <common/uid.h>
#include <vector>

namespace eka2l1 {
    class kernel_system;

    namespace service {
        class property;
    }

    namespace epoc {
        struct window_group;
    }

    namespace drivers {
        class graphics_driver;
    }

    class window_server;
}

namespace eka2l1::epoc::cap {
    static constexpr epoc::uid UIKON_UID = 0x101F8773;
    static constexpr std::uint32_t UIK_CURRENT_HARDWARE_LAYOUT_STATE = 8;
    static constexpr std::uint32_t UIK_PREFERRED_ORIENTATION_KEY = 9;

    class sgc_server {
    public:
        struct wg_state {
            using wg_state_flags = common::ba_t<32>;
            std::uint32_t id_;

            wg_state_flags flags_;
            std::int32_t sp_layout_;
            std::int32_t sp_flags_;
            std::int32_t app_screen_mode_;

            enum flag {
                FLAG_FULLSCREEN = 0,
                FLAG_PARTIAL_FOREGROUND = 1,
                FLAG_UNDERSTAND_PARTIAL_FOREGROUND = 2,
                FLAG_LEGACY_LAYOUT = 3,
                FLAG_ORIENTATION_SPECIFIED = 4,
                FLAG_ORIENTATION_LANDSCAPE = 5
            };

            void set_fullscreen(const bool set);
            bool is_fullscreen() const;

            void set_legacy_layout(const bool set);
            bool is_legacy_layout() const;

            void set_understand_partial_foreground(const bool set);
            bool understands_partial_foreground() const;

            void set_orientation_specified(const bool set);
            bool orientation_specified() const;

            void set_orientation_landspace(const bool set);
            bool orientation_landscape() const;
        };

    private:
        std::vector<wg_state> states_;
        std::vector<std::size_t> focus_callback_handles_;

        service::property *orientation_prop_;
        service::property *hardware_layout_prop_;

        drivers::graphics_driver *graphics_driver_;
        window_server *winserv_;

    public:
        explicit sgc_server();

        bool init(kernel_system *kern, drivers::graphics_driver *driver);
        void change_wg_param(const std::uint32_t id, wg_state::wg_state_flags &flags, const std::int32_t sp_layout,
            const std::int32_t sp_flags, const std::int32_t app_screen_mode);
        void update_screen_state_from_wg(epoc::window_group *group);
        wg_state *get_wg_state(const std::uint32_t wg_id, const bool new_one_if_not_exist);
    };
}