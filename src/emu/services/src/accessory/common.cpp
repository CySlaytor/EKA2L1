#include <services/accessory/common.h>
#include <utils/err.h>

namespace eka2l1::epoc::acc {
    generic_id_array::generic_id_array() {
        for (std::uint32_t i = 0; i < GENERIC_ID_ARRAY_COUNT; i++) {
            ids_[i].header_.dbid_ = epoc::error_not_found;
        }
    }
}