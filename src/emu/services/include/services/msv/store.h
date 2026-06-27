#pragma once

#include <cstdint>
#include <map>
#include <vector>

namespace eka2l1::common {
    class ro_stream;
    class wo_stream;
}

namespace eka2l1::epoc::msv {
    using store_buffer = std::vector<std::uint8_t>;

    class store {
    private:
        std::map<std::uint32_t, store_buffer> stores_;

    public:
        explicit store();

        bool read(common::ro_stream &stream);
        bool write(common::wo_stream &stream);

        store_buffer &buffer_for(const std::uint32_t uid);

        const std::size_t buffer_count() const {
            return stores_.size();
        }
    };
}