#include <services/bluetooth/btmidman.h>
#include <services/bluetooth/protocols/btmidman_inet.h>

namespace eka2l1::epoc::bt {
    midman::midman()
        : local_name_(u"eka2l1")
        , native_handle_(nullptr) {
    }

    std::unique_ptr<midman> make_bluetooth_midman(const eka2l1::config::state &conf, const std::uint32_t reserved_stack_type) {
        return std::make_unique<midman_inet>(conf);
    }
}