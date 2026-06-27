#pragma once

#include <cstdint>

namespace eka2l1::epoc::hwrm::light {
    enum target {
        none = 0,
        primary_display = (1 << 0),
        primary_keyboard = (1 << 1),
        secondary_display = (1 << 2),
        secondary_keyboard = (1 << 3),
        custom_target_1 = (1 << 4),
        custom_target_2 = (1 << 5),
        custom_target_3 = (1 << 6),
        custom_target_4 = (1 << 7),
        custom_target_5 = (1 << 8),
        custom_target_6 = (1 << 9),
        custom_target_7 = (1 << 10),
        custom_target_8 = (1 << 11),
        custom_target_9 = (1 << 12),
        custom_target_10 = (1 << 13),
        custom_target_11 = (1 << 14),
        custom_target_12 = (1 << 15),
        custom_target_13 = (1 << 16),
        custom_target_14 = (1 << 17),
        custom_target_15 = (1 << 18),
        custom_target_16 = (1 << 19),
        custom_target_17 = (1 << 20),
        custom_target_18 = (1 << 21),
        custom_target_19 = (1 << 22),
        custom_target_20 = (1 << 23),
        custom_target_21 = (1 << 24),
        custom_target_22 = (1 << 25),
        custom_target_23 = (1 << 26),
        custom_target_24 = (1 << 27),
        custom_target_25 = (1 << 28),
        custom_target_26 = (1 << 29),
        custom_target_27 = (1 << 30)
    };

    enum status {
        light_status_unk = 0,
        light_status_on = 1,
        light_status_off = 2,
        light_status_blink = 3
    };

    static constexpr std::uint32_t MAXIMUM_LIGHT = 31;
    static constexpr std::uint32_t LIGHT_STATUS_PROP_KEY = 0x2001;
}