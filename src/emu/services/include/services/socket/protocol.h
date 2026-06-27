#pragma once

#include <services/socket/host.h>
#include <services/socket/socket.h>
#include <utils/version.h>

#include <cstdint>
#include <memory>
#include <vector>

namespace eka2l1::epoc::socket {
    enum byte_order {
        byte_order_little_endian = 0,
        byte_order_big_endian = 1,
        byte_order_others = 2
    };

    struct protocol {
    private:
        enum { FLAG_OLDARCH = 1 << 0 };
        std::uint32_t flags_;

    public:
        explicit protocol(const bool oldarch);
        virtual ~protocol() = default;

        const bool is_oldarch() const {
            return flags_ & FLAG_OLDARCH;
        }

        virtual std::u16string name() const = 0;
        virtual std::vector<std::uint32_t> family_ids() const = 0;
        virtual std::vector<std::uint32_t> supported_ids() const = 0;
        virtual epoc::version ver() const = 0;
        virtual byte_order get_byte_order() const = 0;
        virtual std::int32_t message_size() const { return SOCKET_MESSAGE_SIZE_UNDEFINED; }

        virtual std::unique_ptr<host_resolver> make_host_resolver(const std::uint32_t family_id, const std::uint32_t protocol_id) = 0;
        virtual std::unique_ptr<socket> make_socket(const std::uint32_t family_id, const std::uint32_t protocol_id, const socket_type sock_type) = 0;
    };
}