#pragma once

#include <cstdint>

namespace eka2l1 {
    struct socket_client_session;

    namespace service {
        struct ipc_context;
    };
}

namespace eka2l1::epoc::socket {
    enum socket_type {
        socket_type_undefined = 0,
        socket_type_stream = 1,
        socket_type_datagram,
        socket_type_packet,
        socket_type_raw
    };

    enum {
        INVALID_FAMILY_ID = 0xFFFFFFFF
    };

    struct saddress {
        std::uint32_t family_ = INVALID_FAMILY_ID;
        std::uint32_t port_;
        std::uint8_t user_data_[24];
    };

    static constexpr std::uint32_t SOCKET_OPTION_FAMILY_BASE = 1;
    static constexpr std::uint32_t SOCKET_OPTION_ID_NON_BLOCKING_IO = 4;
    static constexpr std::uint32_t SOCKET_OPTION_ID_BLOCKING_IO = 5;

    enum socket_subsession_type {
        socket_subsession_type_host_resolver,
        socket_subsession_type_connection,
        socket_subsession_type_socket,
        socket_subsession_type_net_database
    };

    class socket_subsession {
    protected:
        socket_client_session *parent_;
        std::uint32_t id_;

    public:
        explicit socket_subsession(socket_client_session *parent)
            : parent_(parent) {
        }

        void set_id(const std::uint32_t new_id) {
            id_ = new_id;
        }

        virtual ~socket_subsession() {}

        virtual void dispatch(service::ipc_context *ctx) = 0;
        virtual socket_subsession_type type() const = 0;
    };
}