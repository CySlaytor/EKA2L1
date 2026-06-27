#pragma once
#include <services/bluetooth/btmidman.h>
#include <services/internet/protocols/inet.h>
#include <services/socket/protocol.h>

namespace eka2l1::epoc::bt {
    class sdp_inet_protocol : public socket::protocol {
    private:
        epoc::internet::inet_bridged_protocol *inet_pro_;
        midman *mid_;

    public:
        explicit sdp_inet_protocol(midman *mid, epoc::internet::inet_bridged_protocol *inet_pro, const bool oldarch)
            : socket::protocol(oldarch)
            , inet_pro_(inet_pro)
            , mid_(mid) {}
        std::u16string name() const override { return u"SDP"; }
        std::vector<std::uint32_t> family_ids() const override { return { 0x0101 }; }
        std::vector<std::uint32_t> supported_ids() const override { return { 0x0001 }; }
        epoc::version ver() const override { return epoc::version{ 0, 0, 1 }; }
        epoc::socket::byte_order get_byte_order() const override { return epoc::socket::byte_order_little_endian; }
        std::unique_ptr<epoc::socket::socket> make_socket(const std::uint32_t, const std::uint32_t, const socket::socket_type) override { return nullptr; }
        std::unique_ptr<epoc::socket::host_resolver> make_host_resolver(const std::uint32_t, const std::uint32_t) override { return nullptr; }
    };
}