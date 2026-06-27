#pragma once

#include <cstdint>

namespace eka2l1::epoc::hwrm::vibration {
    enum status {
        status_unk = 0,
        status_not_allowed = 1,
        status_stopped = 2,
        status_on = 3
    };

    static constexpr std::uint32_t VIBRATION_CONTROL_REPO_UID = 0x10200C8B;
    static constexpr std::uint32_t VIBRATION_CONTROL_ENABLE_KEY = 0x01;
    static constexpr std::uint32_t VIBRATION_STATUS_KEY = 0x1001;
}