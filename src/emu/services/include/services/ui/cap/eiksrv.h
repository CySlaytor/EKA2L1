#pragma once

#include <cstdint>

namespace eka2l1 {
    class kernel_system;
    class window_server;

    namespace service {
        class property;
    }
}

namespace eka2l1::epoc::cap {
    enum {
        BATTERY_LEVEL_MAX = 7,
        BATTERY_LEVEL_MIN = 0,
        SIGNAL_LEVEL_MAX = 7,
        SIGNAL_LEVEL_MIN = 0,

        GPRS_SIGNAL_ICON = 0x4,
        COMMON_PACKET_SIGNAL_ICON = 0x104,
        THREEG_SIGNAL_ICON = 0x204
    };

    struct akn_battery_state {
        std::int32_t strength_;
        std::int32_t charging_;
        std::int32_t icon_state_;
        explicit akn_battery_state();
    };

    struct akn_signal_state {
        std::int32_t strength_;
        std::int32_t icon_state_;
        explicit akn_signal_state();
    };

    struct akn_indicator_state {
        enum {
            MAX_VISIBLE_INDICATORS = 10
        };
        std::int32_t visible_indicators_[MAX_VISIBLE_INDICATORS];
        std::int32_t visibile_indicator_states_[MAX_VISIBLE_INDICATORS];
        std::int32_t incall_bubble_flags_;
        bool incall_bubble_allow_in_usual_;
        bool incall_bubble_allow_in_idle_;
        bool incall_bubble_disabled_;
        explicit akn_indicator_state();
    };

    struct akn_status_pane_data {
        std::int32_t foreground_subscriber_id_;
        akn_battery_state battery_;
        akn_signal_state signal_;
        akn_indicator_state indicator_;
        explicit akn_status_pane_data();
    };

    struct eik_status_pane_maintainer {
        service::property *prop_;
        akn_status_pane_data local_data_;
        explicit eik_status_pane_maintainer(kernel_system *kern);
        void publish_data();
    };

    class eik_server {
        eik_status_pane_maintainer status_pane_maintainer_;
        window_server *winserv_;

        enum {
            FLAG_KEY_BLOCK_MODE = 1 << 0
        };
        std::uint32_t flags_;

    public:
        explicit eik_server(kernel_system *kern);
        void init(kernel_system *kern);
        void key_block_mode(const bool is_on);
        const bool key_block_mode() const;

        eik_status_pane_maintainer *get_pane_maintainer() {
            return &status_pane_maintainer_;
        }
    };
};