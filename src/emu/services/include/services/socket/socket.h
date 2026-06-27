#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <utils/reqsts.h>

namespace eka2l1::epoc::socket {
    struct saddress;

    enum {
        SOCKET_MESSAGE_SIZE_IS_STREAM = 0,
        SOCKET_MESSAGE_SIZE_UNDEFINED = 1,
        SOCKET_MESSAGE_SIZE_NO_LIMIT = -1
    };

    struct socket {
    public:
        virtual ~socket() = default;
    };
}