#pragma once

#include <services/socket/common.h>
#include <utils/des.h>

#include <cstdint>
#include <string>
#include <vector>

namespace eka2l1::epoc {
    struct notify_info;
}

namespace eka2l1::epoc::socket {
    struct protocol;

#pragma pack(push, 1)
    struct name_entry {
        enum {
            FLAG_ALIAS_NAME = 1 << 0,
            FLAG_PARTIAL_NAME = 1 << 1
        };

        epoc::buf_static<char16_t, 0x100> name_;
        std::uint32_t length_ = (static_cast<std::uint32_t>(des_type::buf) << 28) | sizeof(saddress);
        std::uint32_t max_length_ = sizeof(saddress);
        saddress addr_;
        std::uint32_t flags_ = 0;
    };
#pragma pack(pop)

    class host_resolver {
    public:
        virtual ~host_resolver() = default;
    };
}