#include <algorithm>
#include <common/chunkyseri.h>
#include <services/audio/keysound/context.h>

namespace eka2l1::epoc::keysound {
    void context::do_it(common::chunkyseri &seri, const std::uint32_t info_count) {
        for (std::uint32_t i = 0; i < info_count; i++) {
            sound_trigger_info info;

            seri.absorb(info.sid);
            seri.absorb(info.key);
            seri.absorb(info.type);

            infos_.push_back(info);
        }

        std::sort(infos_.begin(), infos_.end(), [](const sound_trigger_info &lhs, const sound_trigger_info &rhs) {
            return lhs.key < rhs.key;
        });
    }

    context::context(common::chunkyseri &info_seri, const std::uint32_t uid,
        const std::int32_t resource_id, const std::uint32_t info_count)
        : uid_(uid)
        , rsc_id_(resource_id) {
        do_it(info_seri, info_count);
    }
}