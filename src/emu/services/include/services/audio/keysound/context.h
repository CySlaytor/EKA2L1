#pragma once

#include <cstdint>
#include <vector>

namespace eka2l1::common {
    class chunkyseri;
}

namespace eka2l1::epoc::keysound {
    using sound_id = std::uint16_t;
    static constexpr sound_id NO_SOUND_ID = 1000;

    struct sound_trigger_info {
        sound_id sid;
        std::uint16_t key;
        std::uint8_t type;
    };

    enum sound_type {
        sound_type_file = 0,
        sound_type_sequence = 1,
        sound_type_tone = 2
    };

    struct sound_info {
        std::uint32_t id_;
        std::uint16_t priority_;
        std::uint32_t preference_;

        sound_type type_;
        std::vector<std::uint8_t> sequences_;
        std::uint16_t freq_;
        std::uint32_t duration_;
        std::uint8_t volume_;
    };

    struct context {
        std::vector<sound_trigger_info> infos_;
        std::uint32_t uid_;
        std::int32_t rsc_id_;

        explicit context(common::chunkyseri &info_seri, const std::uint32_t uid,
            const std::int32_t resource_id, const std::uint32_t info_count);

        void do_it(common::chunkyseri &seri, const std::uint32_t info_count);
    };
}