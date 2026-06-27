#include <services/socket/protocol.h>

namespace eka2l1::epoc::socket {
    protocol::protocol(const bool oldarch)
        : flags_(0) {
        if (oldarch)
            flags_ |= FLAG_OLDARCH;
    }
}