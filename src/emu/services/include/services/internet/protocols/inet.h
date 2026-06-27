#pragma once

#include <services/internet/protocols/common.h>
#include <services/socket/protocol.h>
#include <services/socket/socket.h>

#include <common/container.h>
#include <common/sync.h>

#include <utils/des.h>

#include <functional>
#include <memory>
#include <queue>
#include <string>
#include <thread>
#include <vector>

#include <uvlooper/uvlooper.h>

namespace eka2l1 {
    class kernel_system;
}

namespace eka2l1::epoc::internet {
    class inet_bridged_protocol : public socket::protocol {
    private:
        std::shared_ptr<libuv::looper> looper_;
        kernel_system *kern_;

    public:
        explicit inet_bridged_protocol(kernel_system *kern, const bool oldarch);
        void initialize_looper();

        virtual std::u16string name() const override {
            return u"INet";
        }

        std::vector<std::uint32_t> family_ids() const override {
            return { INET_ADDRESS_FAMILY, INET6_ADDRESS_FAMILY };
        }

        virtual std::vector<std::uint32_t> supported_ids() const override {
            return { INET_UDP_PROTOCOL_ID, INET_TCP_PROTOCOL_ID };
        }

        virtual epoc::version ver() const override {
            epoc::version v;
            v.major = 0;
            v.minor = 0;
            v.build = 1;
            return v;
        }

        virtual epoc::socket::byte_order get_byte_order() const override {
            return epoc::socket::byte_order_little_endian;
        }

        virtual std::unique_ptr<epoc::socket::socket> make_socket(const std::uint32_t family_id, const std::uint32_t protocol_id, const socket::socket_type sock_type) override {
            return nullptr;
        }

        virtual std::unique_ptr<epoc::socket::host_resolver> make_host_resolver(const std::uint32_t family_id, const std::uint32_t protocol_id) override {
            return nullptr;
        }

        kernel_system *get_kernel_system() {
            return kern_;
        }

        std::shared_ptr<libuv::looper> get_looper() {
            return looper_;
        }
    };
}